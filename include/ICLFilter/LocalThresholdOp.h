#ifndef LOCAL_THRESHOLD_OP_H
#define LOCAL_THRESHOLD_OP_H

#include <ICLFilter/UnaryOp.h>
#include <ICLUtils/Size.h>
#include <ICLFilter/IntegralImgOp.h>
#include <vector>
#include <ICLUtils/Uncopyable.h>

namespace icl{
  
  /// LocalThreshold Filter class \ingroup UNARY
  /** This implementation of a local threshold bases on the calculation of an integral image of
      the given input image.
       \f[
      A(i,j) = \sum\limits_{x=0}^i \sum\limits_{y=0}^j  a(i,j)
      \f]
      The integral image can be calculated incrementally using the following equation:
      \f[
      A(i,j) = a(i,j)+A(i-1,j)+A(i,j-1)-A(i-1,j-1)
      \f]
      
      Once having access to the integral image data, it is possible to calculate the mean of an
      abritrary rectangular image region in constant time. Look at the following ASCII example:
      <pre>
      considere the following rectangular image region decribed by 4 points A,B,C and D
      
      .C....A...
      ..+++++...
      ..+++++...
      ..+++++...
      ..+++++...    
      .B++++D...
      ..........
      
      
      -the "mass" of pixels inside the rect can be obtained by calculating
      
      X = D - A - B + C
      
      
      -the pixel count in the rogion is 

      P = (A.x-C.x) * (B.y-C.y) 

      
      - so the region mean can be calculated by evaluating
       
      Mean = X/P
      </pre>
      
      \section T__ Threshold
      A local image threshold at an image location \f$p=(px,py)\f$ must factor in the local image intensity, which can be 
      approximated by the mean value \f$\mu_p\f$ of the square region centered at \f$p\f$ with a certain radius \f$r\f$. 
      To emphazise that \f$\mu_p\f$ depends also on \f$r\f$ we denote it by \f$\mu_r{p}\f$.
      To have more influence on the actually used threshold at location \f$p\f$, we used an additive global threshold 
      variable \f$g\f$.
      
      \section A__ Algorithm
      <pre>
      Input: Input-Image i, region radius r, global threshold g, destination image d
      
      1. I := integral image of i
      2. (static) calculate region size image S
         This is nessesary because border regions do not have the expected region size
         (2*r+1)² due to the overlap with the image border. The calculation of S is not
         very complex, so it is not discussed here further.
      3. I := extend (I). To avoid accessing invalid pixel locations of the integral image,
         it is enlarged to each edge by r. To garantee, that this optimization does not lead
         to incorrect results, the upper and the left border of the integral image is filled with
         zeros, and the lower and the right edge is filled with the value of the nearest non-
         border pixel.
      4. Pixel Loop:
         For each pixel (x,y) of i
            4.1 Estimate the corresponding edge points A,B,C and D of the squared neighbourhood.
                (do not forget that the integral image has got a r-pixle border to each edge)
                C := (x-r,y-r)
                A := (x+r,y-r)     
                B := (x-1,y+r)
                D := (x+r,y+r)
            4.2 Calculate the region mean with respect to the underlying region size
                M = (I(D) - I(A) - I(B) + I(C))/S(x,y)
            4.3 Calculate the theshold operation
                d(x,y) = 255 * (i(x,y) > (M+g) )
          endfor
      </pre>

      \section M__ Mutli channel images
      This time, no special operation for multi channels images are implemneted, so each channel
      is process independently in this case.
  
      
      \section GAMMA Experimental feature gamma slope
      By applying a small adaption, the procedure presented avoid can be used to 
      apply a local gamma correction on a source image. We just have to exchange the "stair"-function
      above \f$ d(x,y) = 255 * (i(x,y) > (M+g) ) \f$ by a linear function:
      
      <pre>
      using a linear function function f(x) = m*x + b    (with clipping to range [0,255])
      with m = gammaSlope (new variable)
           k = M+g
           that f(k) = 128
         
         f(x) = m(x-k)+128     (clipped to [0,255])
      </pre>
  */
  class LocalThresholdOp : public UnaryOp, public Uncopyable{
    public:
    
    /// create a new LocalThreshold object with given mask-size and global threshold
    /** @param maskSize size of the mask to use for calculations, the image width and
                        height must each be larger than 2*maskSize.
        @param globalThreshold additive Threshold to the calculated reagions mean
        @param gammaSlope gammaSlope (Default=0) (*Experimental feature*)
                          if set to 0 (default) the binary threshold is used
    */
    LocalThresholdOp(unsigned int maskSize=10, int globalThreshold=0, float gammaSlope=0);
    
    /// virtual apply function 
    /** roi support is realized by copying the current input image ROI into a 
        dedicate image buffer with no roi set
    **/
    virtual void apply(const ImgBase *src, ImgBase **dst);

    /// disables multi-threading for this UnaryOp implementation (writes an error message)
    virtual void applyMT(const ImgBase *src, ImgBase **dst, int nThreads){
      (void)src;(void)dst;(void)nThreads;
      ERROR_LOG("multi threading is not yet supported for this class");
    }
    
    /// Import unaryOps apply function without destination image
    UnaryOp::apply;
    
    /// set a new mask size (a new mask size image must be calculate in the next apply call)
    void setMaskSize(unsigned int maskSize);
    
    /// sets a enw global threshold value to used
    void setGlobalThreshold(int globalThreshold);

    /// sets a new gamma slope to used (if gammaSlope is 0), the binary threshold is used
    void setGammaSlope(float gammaSlope);
    
    /// returns the current mask size
    unsigned int getMaskSize() const;

    /// returns the current global threshold value
    int getGlobalThreshold() const;

    /// returns the current gamma slope
    float getGammaSlope() const; 

    private:
    /// mask size 
    unsigned int m_uiMaskSize;

    /// global threshold
    int m_iGlobalThreshold;

    /// gamma slope
    float m_fGammaSlope;
    
    /// ROI buffer image for ROI support
    ImgBase *m_poROIImage;

    /// mask size the corresponds to the current ROI size image
    unsigned int m_uiROISizeImagesMaskSize;

    /// ROI size image
    Img32s m_oROISizeImage;
    
    /// internally used integral image buffer (int)
    Img32s m_oIntegralImage;

    /// internally used integral image buffer (float)
    Img32f m_oIntegralImageF;
  };
  
}  
#endif