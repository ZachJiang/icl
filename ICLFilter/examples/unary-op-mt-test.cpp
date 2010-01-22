#include <ICLQuick/Quick.h>
#include <ICLFilter/UnaryArithmeticalOp.h>
#include <ICLFilter/ConvolutionOp.h>
#include <ICLFilter/LocalThresholdOp.h>
#include <ICLUtils/ProgArg.h>

int main(int n, char **ppc){
  pa_explain("-n","number of threads as int");
  pa_init(n,ppc,"-n(1)");
  ImgQ image = create("parrot");
  
  ImgBase *dst = 0;

  ERROR_LOG("multi-threaded filtering does not work correctly yet!");
  // UnaryArithmeticalOp op(UnaryArithmeticalOp::addOp, 100);
  //  ConvolutionOp op(ConvolutionOp::kernelSobelX5x5);
  //LocalThresholdOp op(30,0,0);
  //  float kernel[25];
  //ConvolutionOp op(kernel,Size(5,5));
  ConvolutionOp op(ConvolutionKernel(ConvolutionKernel::sobelX5x5));
  
  int nt = pa_subarg<int>("-n",0,1);
  printf("applying with %d threads \n",nt);
  tic();
  for(int i=0;i<1000;i++){
    op.applyMT(&image,&dst,nt);
  }
  toc();
  
  show(cvt(dst));
  return 0;
}

