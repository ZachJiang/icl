#include <ICLFilter/OpROIHandler.h>
#include <ICLUtils/Macros.h>

namespace icl {

   /// check+adapt destination images parameters against given values
  bool OpROIHandler::prepare (ImgBase **ppoDst, depth eDepth, 
                              const Size &imgSize, format eFormat, int nChannels, 
                              const Rect& roi, Time timestamp) {
    ICLASSERT_RETURN_VAL (ppoDst, false);
    if (m_bCheckOnly) {
      ImgBase* dst = *ppoDst;
      ICLASSERT_RETURN_VAL( dst , false);
      ICLASSERT_RETURN_VAL( dst->getDepth() == eDepth , false);
      ICLASSERT_RETURN_VAL( dst->getChannels () == nChannels ,false);
      ICLASSERT_RETURN_VAL( dst->getFormat () == eFormat ,false);
      if(dst->getROISize() != roi.getSize()){
        ERROR_LOG("ROI size missmatch: given: "<< roi.getSize() 
                  << "  destination: "<< dst->getROISize());
        return false;
      }
      
      dst->setTime(timestamp);
    } else {
      ensureCompatible (ppoDst, eDepth, imgSize, 
                        nChannels, eFormat, roi);
      (*ppoDst)->setTime(timestamp);
    }
    return true;
  }
}