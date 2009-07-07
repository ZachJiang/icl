#ifndef RANGE_H
#define RANGE_H

#include <iclBasicTypes.h>
#include <iclClippedCast.h>
#include <iclMacros.h>

#include <algorithm>
#include <limits>
#include <iostream>
#include <string>

namespace icl{
  
 
  /// class representing a range defined by min and max value \ingroup TYPES
  template<class Type> 
  struct Range{

    /** \cond */

    template<class T> struct MinMaxFunctor{
    MinMaxFunctor(Range<T> *r):r(r){}
      Range<T> *r;
      inline void operator()(const T &x){
        r->minVal = std::min(r->minVal,x);
        r->maxVal = std::max(r->maxVal,x);
      }
    };
    /** \endcond */
  
    virtual ~Range(){}
    /// create an empty range (min = max = 0)
    Range():minVal(Type(0)),maxVal(Type(0)){}
    
    /// create a special Range
    Range(Type minVal, Type maxVal): minVal(minVal), maxVal(maxVal){}

    /// returns type range (using std::numeric_limits<Type>)
    static inline Range<Type> limits(){
      return Range<Type>(std::numeric_limits<Type>::min(),std::numeric_limits<Type>::max());
    }
    
    /// estimate range of given iterator range (using std::for_each)
    static inline Range<Type> from(const Type *begin,const Type *end){
      Range<Type> r = Range<Type>::limits();
      std::swap(r.minVal,r.maxVal);
      std::for_each(begin,end,MinMaxFunctor<Type>(&r));
      return r;
    }

    /// minimum value of this range
    Type minVal;
    
    /// maximum value of this range
    Type maxVal;
    
    /// return max-min
    Type getLength() const { return maxVal - minVal; } 
    
    /// casts this range into another depth
    template<class dstType>
    const Range<dstType> castTo() const{
      return Range<dstType>(clipped_cast<Type,dstType>(minVal),clipped_cast<Type,dstType>(maxVal));
    }
    
    /// tests whether a given value is inside of this range
    virtual bool contains(Type value) const { return value >= minVal && value <= maxVal; }
    
    // camparison operator
    bool operator==(const Range<Type> &other) const{
      return minVal==other.minVal && maxVal == other.maxVal;
    }
    
    // camparison operator
    bool operator!=(const Range<Type> &other) const{
      return minVal!=other.minVal || maxVal != other.maxVal;
    }

  };
  
#define ICL_INSTANTIATE_DEPTH(D) typedef Range<icl##D> Range##D;
  ICL_INSTANTIATE_ALL_DEPTHS
#undef ICL_INSTANTIATE_DEPTH

  /// puts a string representation [min,max] of given range into the given stream
  /** Available for all icl-Types (icl8u,icl16s, icl32s, icl32f and icl64f and
      for unsigned int */
  template<class T> 
  std::ostream &operator<<(std::ostream &s, const Range <T> &range);

  /// parses a range argument into a std::string
  template<class T> 
  std::istream &operator>>(std::istream &s, Range <T> &range);


}
#endif