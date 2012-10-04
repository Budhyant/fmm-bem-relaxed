/*
Copyright (C) 2011 by Rio Yokota, Simon Layton, Lorena Barba

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
THE SOFTWARE.
*/

#pragma once

#include <Types.hpp>
#include <Vec.hpp>
#include <Octree.hpp>
#include <TransformIterator.hpp>

//! Interface between tree and kernel
template <class Kernel>
class SimpleEvaluator
{
public:
  //! Point type
  typedef typename Kernel::point_type point_type;
  //! Multipole expansion type
  typedef typename Kernel::multipole_type multipole_type;
  //! Local expansion type
  typedef typename Kernel::local_type local_type;
  //! Kernel source type
  typedef typename Kernel::charge_type charge_type;
  //! Kernel result type
  typedef typename Kernel::result_type result_type;

private:
  //! Kernel
  Kernel& K;
  //! Multipole expansions corresponding to Box indices in Octree
  std::vector<multipole_type> M;
  //! Local expansions corresponding to Box indices in Octree
  std::vector<local_type> L;

  typename std::vector<result_type>::iterator results_begin;
  typename std::vector<charge_type>::const_iterator charges_begin;

public:
  //! Constructor
  SimpleEvaluator() : K(Kernel()), M(0), L(0) {};
  SimpleEvaluator(Kernel& k) : K(k), M(0), L(0) {};
  //! Destructor
  ~SimpleEvaluator() {}

  // upward sweep using new tree structure
  void upward(Octree<point_type>& otree, const std::vector<charge_type>& charges)
  {
    M.resize(otree.boxes());
    L.resize(otree.boxes());

    unsigned lowest_level = otree.levels();
    printf("lowest level in tree: %d\n",(int)lowest_level);

    // For the lowest level up to the highest level
    for (unsigned l = otree.levels()-1; l != 0; --l) {
      // For all boxes at this level
      auto b_end = otree.box_end(l);
      for (auto bit = otree.box_begin(l); bit != b_end; ++bit) {
        auto box = *bit;

        // Initialize box data
        unsigned idx = box.index();
        double box_size = box.side_length();
        K.init_multipole(M[idx], box_size);
        K.init_local(L[idx], box_size);

        if (box.is_leaf()) {
          // If leaf, make P2M calls
          auto body2point = [](typename Octree<point_type>::Body b) { return b.point(); };

          auto p_begin = make_transform_iterator(box.body_begin(), body2point);
          auto p_end   = make_transform_iterator(box.body_end(),   body2point);
          auto c_begin = charges.begin()+box.body_begin()->index();

          printf("P2M: box: %d\n", (int)box.index());
          K.P2M(p_begin, p_end, c_begin, box.center(), M[idx]);

        } else {
          // If not leaf, make M2M calls

          // For all the children, M2M
          auto c_end = box.child_end();
          for (auto cit = box.child_begin(); cit != c_end; ++cit) {
            auto cbox = *cit;
            auto translation = box.center() - cbox.center();

            printf("M2M: %d to %d\n", cbox.index(), idx);
            K.M2M(M[cbox.index()], M[idx], translation);
          }
        }
      }
    }
  }

  template <typename BOX, typename Q>
  void interact(const BOX& b1, const BOX& b2, Q& pairQ) {
    point_type r0 = b1.center() - b2.center();
    double r0_norm = std::sqrt(norm(r0));
    //printf("r0_norm = %f, THETA = %f, D = %f\n", r0_norm, THETA, b1.side_length() + b2.side_length());
    printf("r0_norm*THETA: %lg, rhs: %lg\n",r0_norm*THETA,b1.side_length() + b2.side_length());
    if (r0_norm * THETA > b1.side_length() + b2.side_length()) {
      // These boxes satisfy the multipole acceptance criteria
#if HYBRID
      if( timeP2P*Cj->NDLEAF < timeM2P && timeP2P*Ci->NDLEAF*Cj->NDLEAF < timeM2L) {// If P2P is fastest
        //evalP2P(b1,b2);                                           //  Evaluate on CPU, queue on GPU
      } else if ( timeM2P < timeP2P*Cj->NDLEAF && timeM2P*Ci->NDLEAF < timeM2L ) {// If M2P is fastest
        //evalM2P(b1,b2);                                           //  Evaluate on CPU, queue on GPU
      } else {                                                    // If M2L is fastest
        //evalM2L(b1,b2);                                           //  Evaluate on CPU, queue on GPU
      }                                                           // End if for kernel selection
#elif TREECODE
      evalM2P(b1,b2);                                             // Evaluate on CPU, queue on GPU
#else
      //evalM2L(b1,b2);                                             // Evalaute on CPU, queue on GPU
#endif
    } else if(b1.is_leaf() && b2.is_leaf()) {
      printf("P2P: %d to %d\n",b1.index(),b2.index());
      evalP2P(b1,b2);
    } else {
      pairQ.push_back(std::make_pair(b1,b2));
    }
  }


  void downward(Octree<point_type>& octree,
                const std::vector<charge_type>& charges,
                std::vector<result_type>& results) {

    // keep references to charges & results
    charges_begin = charges.begin();
    results_begin = results.begin();

    typedef typename Octree<point_type>::Box Box;
    typedef typename std::pair<Box, Box> box_pair;
    std::deque<box_pair> pairQ;

    // Queue based tree traversal for P2P, M2P, and/or M2L operations
    pairQ.push_back(box_pair(octree.root(), octree.root()));

    while (!pairQ.empty()) {
      box_pair boxes = pairQ.front();
      pairQ.pop_front();
      bool is_leaf1 = boxes.first.is_leaf();
      bool is_leaf2 = boxes.second.is_leaf();

      if (is_leaf2 || (!is_leaf1 && boxes.first > boxes.second)) {
        // Split the first box into children and interact
        auto c_end = boxes.first.child_end();
        for (auto cit = boxes.first.child_begin(); cit != c_end; ++cit)
          interact(*cit, boxes.second, pairQ);
      } else {
        // Split the second box into children and interact
        auto c_end = boxes.second.child_end();
        for (auto cit = boxes.second.child_begin(); cit != c_end; ++cit)
          interact(boxes.first, *cit, pairQ);
      }
    }

    //

    /*
    // For the highest level down to the lowest level
    for (unsigned l = 2; l < octree.levels(); ++l) {
      // For all boxes at this level
      auto b_end = octree.box_end(l);
      for (auto bit = octree.box_begin(l); bit != b_end; ++bit) {
        auto box = *bit;
        unsigned idx = box.index();

        // Initialize box data
        if (box.is_leaf()) {
          // If leaf, make L2P calls

          // For all the bodies, L2P
          auto p_begin = box.body_begin();

          auto body2point = [](typename Octree<point_type>::Body b) { return b.point(); };
          auto t_begin = make_transform_iterator(box.body_begin(), body2point);
          auto t_end   = make_transform_iterator(box.body_end(), body2point);
          auto r_begin = results_begin + box.index();

          K.L2P(t_begin, t_end, r_begin,
                box.center(),
                L[idx]);
        } else {
          // If not leaf, make L2L calls

          // For all the children, L2L
          auto c_end = box.child_end();
          for (auto cit = box.child_begin(); cit != c_end; ++cit) {
            auto cbox = *cit;
            auto translation = cbox.center() - box.center();

            K.L2L(L[idx], L[cbox.index()], translation);
          }
        }
      }
    }
    */
  }

  void evalP2P(const typename Octree<point_type>::Box& b1,
               const typename Octree<point_type>::Box& b2) {
    // Point iters
    auto body2point = [](const typename Octree<point_type>::Body& b) { return b.point(); };
    auto p1_begin = make_transform_iterator(b1.body_begin(), body2point);
    auto p1_end   = make_transform_iterator(b1.body_end(),   body2point);
    auto p2_begin = make_transform_iterator(b2.body_begin(), body2point);
    auto p2_end   = make_transform_iterator(b2.body_end(),   body2point);

    // Charge iters
    auto c1_begin = charges_begin + b1.body_begin()->index();
    auto c2_begin = charges_begin + b2.body_begin()->index();

    // Result iters
    auto r1_begin = results_begin + b1.body_begin()->index();
    auto r2_begin = results_begin + b2.body_begin()->index();

    K.P2P(p1_begin, p1_end, c1_begin,
          p2_begin, p2_end, c2_begin,
          r1_begin, r2_begin);
  }

  void evalM2P(const typename Octree<point_type>::Box& b1,
               const typename Octree<point_type>::Box& b2)
  {
    // Target point iters
    auto body2point = [](const typename Octree<point_type>::Body& b) { return b.point(); };
    auto t_begin = make_transform_iterator(b2.body_begin(), body2point);
    auto t_end   = make_transform_iterator(b2.body_end(), body2point);

    // Target result iters
    auto r_begin = results_begin + b2.body_begin()->index();

    auto idx = b1.index();
    printf("calling K.M2P: %d to %d\n", b1.index(), b2.index());
    K.M2P(b1.center(), M[idx], t_begin, t_end, r_begin);
  }

  static void evalP2P(Kernel& K, Bodies& sources, Bodies& targets)
  {
    // quiet warnings
    (void)K;
    (void)sources;
    (void)targets;
  }
};
