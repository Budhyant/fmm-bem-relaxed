#pragma once

#include <iostream>
#include <algorithm>
#include <math.h>

/** @file BoundingBox.hpp
 * @brief Define the BoundingBox class for ND bounding boxes. */

/** @class BoundingBox
 * @brief Class representing ND bounding boxes.
 *
 * A BoundingBox is a ND volume. Its fundamental operations are contains(),
 * which tests whether a point is in the volume, and operator+=(), which
 * extends the volume as necessary to ensure that the volume contains a point.
 *
 * BoundingBoxes are implemented as boxes -- ND rectangular cuboids -- whose
 * sides are aligned with the principal axes.
 */

// TODO: Optimize for cubes?
template <typename Point>
class BoundingBox {
 public:
  typedef Point point_type;

  /** Construct an empty bounding box. */
  BoundingBox()
    : empty_(true) {
  }
  /** Construct the minimal bounding box containing @a p.
   * @post contains(@a p) && min() == @a p && max() == @a p */
  explicit BoundingBox(const Point& p)
    : empty_(false), min_(p), max_(p) {
  }
  /** Construct the minimal bounding box containing a given sphere.
   * @param[in] center center of the sphere
   * @param[in] radius radius of the sphere */
  BoundingBox(const Point& center, double radius)
    : empty_(false) {
    radius = fabs(radius);
    min_ = center - radius;
    max_ = center + radius;
  }
  /** Construct the minimal bounding box containing @a p1 and @a p2.
   * @post contains(@a p1) && contains(@a p2) */
  BoundingBox(const Point& p1, const Point& p2)
    : empty_(false), min_(p1), max_(p1) {
    *this |= p2;
  }
  /** Construct a bounding box containing the points in [first, last). */
  template <typename IT>
  BoundingBox(IT first, IT last)
    : empty_(true) {
    insert(first, last);
  }

  /** Test if the bounding box is empty (contains no points). */
  bool empty() const {
    return empty_;
  }
  /** Test if the bounding box is nonempty.
   *
   * This function lets you write code such as "if (b) { ... }" or
   * "if (box1 & box2) std::cout << "box1 and box2 intersect\n". */
  operator const void*() const {
    return empty_ ? 0 : this;
  }

  /** Return the minimum corner of the bounding box.
   * @post empty() || contains(min())
   *
   * The minimum corner has minimum x, y, and z coordinates of any corner.
   * An empty box has min() == Point(). */
  const Point& min() const {
    return min_;
  }
  /** Return the maximum corner of the bounding box.
   * @post empty() || contains(max()) */
  const Point& max() const {
    return max_;
  }
  /** Return the dimensions of the bounding box.
   * @return max() - min()
   */
  Point dimensions() const {
    return max_ - min_;
  }
  /** Return the center of the bounding box. */
  Point center() const {
    return (min_ + max_) / 2;
  }

  /** Test if point @a p is in the bounding box. */
  bool contains(const Point& p) const {
    if (empty())
      return false;
    for (unsigned i=0; i!=Point::dimension; ++i)
      if (p[i] < min_[i] || p[i] > max_[i])
        return false;
    return true;
  }
  /** Test if @a box is entirely within this bounding box.
   *
   * Returns false if @a box.empty(). */
  bool contains(const BoundingBox& box) const {
    return !box.empty() && contains(box.min()) && contains(box.max());
  }
  /** Test if @a box intersects this bounding box. */
  bool intersects(const BoundingBox& box) const {
    if (empty() || box.empty())
      return false;
    for (unsigned i=0; i!=Point::dimension; ++i)
      if (box.min_[i] > max_[i] || box.max_[i] < min_[i])
        return false;
    return true;
  }

  /** Extend the bounding box to contain @a p.
   * @post contains(@a p) is true
   * @post if old contains(@a x) was true, then new contains(@a x) is true */
  BoundingBox& operator|=(const Point& p) {
    if (empty_) {
      empty_ = false;
      min_ = max_ = p;
    } else {
      for (unsigned i=0; i!=Point::dimension; ++i) {
        min_[i] = std::min(min_[i], p[i]);
        max_[i] = std::max(max_[i], p[i]);
      }
    }
    return *this;
  }
  /** Extend the bounding box to contain @a box.
   * @post contains(@a box) is true
   * @post if old contains(@a x) was true, then new contains(@a x) is true */
  BoundingBox& operator|=(const BoundingBox& box) {
    if (!box.empty())
      (*this |= box.min()) |= box.max();
    return *this;
  }
  /** Extend the bounding box to contain the points in [first, last). */
  template <typename IT>
  BoundingBox& insert(IT first, IT last) {
    while (first != last) {
      *this |= *first;
      ++first;
    }
    return *this;
  }

  /** Intersect this bounding box with @a box. */
  BoundingBox &operator&=(const BoundingBox& box) {
    if (empty() || box.empty())
      goto erase;
    for (int i = 0; i!=Point::dimension; ++i) {
      if (min_[i] > box.max_[i] || max_[i] < box.min_[i])
	goto erase;
      if (min_[i] < box.min_[i])
	min_[i] = box.min_[i];
      if (max_[i] > box.max_[i])
	max_[i] = box.max_[i];
    }
    return *this;
 erase:
    clear();
    return *this;
  }

  /** Clear the bounding box.
   * @post empty() */
  void clear() {
    empty_ = true;
    min_ = max_ = Point();
  }

 private:
  bool empty_;
  Point min_;
  Point max_;
};

/** Write a BoundingBox to an output stream.
 *
 * An empty BoundingBox is written as "[]". A nonempty BoundingBox is
 * written as "[minx miny minz:maxx maxy maxz]", where all coordinates
 * are double-precision numbers.
 */
template <typename P>
inline std::ostream& operator<<(std::ostream& s, const BoundingBox<P>& box) {
  if (box.empty())
    return (s << '[' << ']');
  else
    return (s << '[' << box.min() << ':' << box.max() << ']');
}

/** Return a bounding box that contains @a box and @a p. */
template <typename P>
BoundingBox<P> operator|(BoundingBox<P> box, const P& p) {
  return box |= p;
}
/** Return the union of @a box1 and @a box2. */
template <typename P>
BoundingBox<P> operator|(BoundingBox<P> box1, const BoundingBox<P>& box2) {
  return box1 |= box2;
}
/** Return a bounding box that contains @a p1 and @a p2. */
template <typename P>
BoundingBox<P> operator|(const P& p1, const P& p2) {
  return BoundingBox<P>(p1, p2);
}
/** Return the intersection of @a box1 and @a box2. */
template <typename P>
BoundingBox<P> operator&(BoundingBox<P> box1, const BoundingBox<P>& box2) {
  return box1 &= box2;
}