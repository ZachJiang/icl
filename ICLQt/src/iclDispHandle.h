#ifndef ICL_DISP_HANDLE_H
#define ICL_DISP_HANDLE_H

#include <iclSimpleMatrix.h>
#include <iclLabelHandle.h>
#include <iclGUIHandle.h>

namespace icl{
  
  /** \cond */
  struct LabelHandleAlloc{
    static LabelHandle create() { return LabelHandle(); }
  };
  /** \endcond */
  
  /// Type definition for handling GUI-"disp" components \ingroup HANDLES
  /** @see GUI, SimpleMatrix template class of the ICLUtils package */
  typedef SimpleMatrix<LabelHandle,LabelHandleAlloc>  LabelMatrix;

  /// Handle class for disp components \indgoup HANDLE
  class DispHandle : public GUIHandle<LabelMatrix>{
    public:
    /// Create a new DispHandle with given 
    DispHandle(LabelMatrix *lm=0) : GUIHandle<LabelMatrix>(lm){}
    
    /// column access operator (use h[x][y] to access elements)
    inline LabelHandle *operator[](int x){ return (***this)[x]; }

    /// column access operator (use h[x][y] to access elements)
    inline LabelHandle *operator[](int x) const{ return (***this)[x]; }

    /// width of the matrix (max x index is w()-1
    inline int w() const { return (***this).w(); }

    /// height of the matrix (max y index is h()-1
    inline int h() const { return (***this).h(); }

    /// returns the number of matrix elements w*h
    inline int dim() const { return (***this).dim(); }
  
  };

}

#endif
