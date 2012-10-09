#pragma once

/**
 * Base class for all evaluators
 */

#include <EvaluatorFMM.hpp>
#include <EvaluatorTreecode.hpp>

template <class Tree, class Kernel>
class EvaluatorBase
{
 public:
  //! Kernel type
  typedef Kernel kernel_type;
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

 protected:
  //! Octree
  Tree& tree;
  //! Kernel
  Kernel& K;

  //! Multipole expansions corresponding to Box indices in Octree
  std::vector<multipole_type> M;
  //! Local expansions corresponding to Box indices in Octree
  std::vector<local_type> L;
  
  // THETA for multipole acceptance criteria
  double THETA;
 private:
  typename std::vector<result_type>::iterator results_begin;
  typename std::vector<charge_type>::const_iterator charges_begin;

 public:
  EvaluatorBase(Tree& t, Kernel& k, double theta) : tree(t), K(k), THETA(theta) {};

  //! Upward sweep
  virtual void upward(const std::vector<charge_type>& charges) = 0;
  //! 'Interaction' stage
  virtual void interactions(std::vector<result_type>& results) = 0;
  //! Downward sweep
  virtual void downward(std::vector<result_type>& results) = 0;

  //! Abstract factory
  static EvaluatorBase<Tree,Kernel> *createEvaluator(Tree& t, Kernel& k, FMM_options& options)
  {
    if (options.evaluator == FMM)
    {
      return new EvaluatorFMM<Tree,Kernel>(t,k,options.THETA);
    }
    else if (options.evaluator == TREECODE)
    {
      return new EvaluatorTreecode<Tree,Kernel>(t,k,options.THETA);
    }
    else
    {
      return NULL;
    }
  }

  //! set the value of THETA
  void setTheta(double th) {
    THETA = th;
  }
  //! get the value of THETA
  double getTheta() {
    return THETA;
  }

  //! name of the evaluator
  virtual std::string name() = 0;
};

