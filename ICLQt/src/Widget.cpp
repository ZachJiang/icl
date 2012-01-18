/********************************************************************
**                Image Component Library (ICL)                    **
**                                                                 **
** Copyright (C) 2006-2010 CITEC, University of Bielefeld          **
**                         Neuroinformatics Group                  **
** Website: www.iclcv.org and                                      **
**          http://opensource.cit-ec.de/projects/icl               **
**                                                                 **
** File   : ICLQt/src/Widget.cpp                                   **
** Module : ICLQt                                                  **
** Authors: Christof Elbrechter                                    **
**                                                                 **
**                                                                 **
** Commercial License                                              **
** ICL can be used commercially, please refer to our website       **
** www.iclcv.org for more details.                                 **
**                                                                 **
** GNU General Public License Usage                                **
** Alternatively, this file may be used under the terms of the     **
** GNU General Public License version 3.0 as published by the      **
** Free Software Foundation and appearing in the file LICENSE.GPL  **
** included in the packaging of this file.  Please review the      **
** following information to ensure the GNU General Public License  **
** version 3.0 requirements will be met:                           **
** http://www.gnu.org/copyleft/gpl.html.                           **
**                                                                 **
** The development of this software was supported by the           **
** Excellence Cluster EXC 277 Cognitive Interaction Technology.    **
** The Excellence Cluster EXC 277 is a grant of the Deutsche       **
** Forschungsgemeinschaft (DFG) in the context of the German       **
** Excellence Initiative.                                          **
**                                                                 **
*********************************************************************/

// {{{ includes

#include <ICLQt/Widget.h>
#include <ICLCore/Img.h>
#include <ICLQt/GLImg.h>
#include <ICLQt/GLPaintEngine.h>
#include <ICLIO/GenericImageOutput.h>
#include <string>
#include <vector>
#include <ICLUtils/Time.h>
#include <ICLUtils/ProgArg.h>
#include <ICLQt/QImageConverter.h>
#include <ICLCC/FixedConverter.h>
#include <QtGui/QImage>

#include <ICLQt/IconFactory.h>
#include <ICLUtils/Thread.h>

#include <QtGui/QPainter>
#include <QtCore/QTimer>
#include <QtGui/QLabel>
#include <QtGui/QInputDialog>
#include <QtCore/QMutexLocker>
#include <QtGui/QPushButton>
#include <QtGui/QHBoxLayout>
#include <QtGui/QComboBox>
#include <QtGui/QSlider>
#include <QtGui/QSizePolicy>
#include <QtGui/QFileDialog>
#include <QtGui/QSpinBox>
#include <QtGui/QMouseEvent>
#include <QtGui/QPaintEvent>
#include <QtGui/QWheelEvent>
#include <QtGui/QColorDialog>
#include <QtGui/QMessageBox>
#include <QtGui/QTextEdit>


#include <ICLQt/GUI.h>
#include <ICLQt/TabHandle.h>
#include <ICLQt/BoxHandle.h>
#include <ICLQt/SplitterHandle.h>
#include <ICLQt/LabelHandle.h>
#include <ICLQt/ButtonHandle.h>
#include <ICLQt/ComboHandle.h>
#include <ICLQt/StringHandle.h>
#include <ICLQt/SpinnerHandle.h>
#include <ICLQt/SliderHandle.h>
#include <ICLUtils/StringUtils.h>


#include <ICLUtils/Rect32f.h>
#include <ICLIO/File.h>
#include <ICLIO/FileWriter.h>
#include <ICLUtils/Range.h>
#include <ICLCore/Types.h>

#include <ICLQt/ThreadedUpdatableSlider.h>
#include <ICLQt/HistogrammWidget.h>

// }}}

using namespace std;
namespace icl{

#define LOCK_SECTION QMutexLocker SECTION_LOCKER(&m_data->mutex)

  class ZoomAdjustmentWidgetParent;
  
  struct OSDGLButton{
    // {{{ open

    enum IconType {Tool,Zoom,Lock, Unlock,RedZoom,RedCam,
                   NNInter, LINInter, CustomIcon, RangeNormal, RangeScaled};
    enum Event { Move, Press, Release, Enter, Leave,Draw};

    std::string id;
    Rect bounds;
    bool toggable;
    Img8u icon;    
    Img8u downIcon;
    
    bool over;
    bool down;
    bool toggled;
    bool visible;

    GLImg tmImage;

    Function<void> vcb;
    Function<void,bool> bcb;                
                  
    static inline const Img8u &get_icon(IconType icon){
      switch(icon){
        case Tool: return IconFactory::create_image("tool");
        case Zoom: return IconFactory::create_image("zoom");
        case RedZoom: return IconFactory::create_image("red-zoom");
        case Unlock: return IconFactory::create_image("detached");
        case Lock: return IconFactory::create_image("embedded");
        case RedCam: return IconFactory::create_image("red-camera");
        case NNInter: return IconFactory::create_image("inter-nn");
        case LINInter: return IconFactory::create_image("inter-lin");
        case RangeNormal: return IconFactory::create_image("range-normal");
        case RangeScaled: return IconFactory::create_image("range-scaled");
        case CustomIcon: return IconFactory::create_image("custom");
      }
      static Img8u undef(Size(32,32),4);
      return undef;
    }
    
    OSDGLButton(const std::string &id, int x, int y, int w, int h,const ImgBase *icon, const Function<void> &cb=Function<void>()):
      id(id),bounds(x,y,w,h),toggable(false),over(false),down(false),
      toggled(false),visible(false),vcb(cb){
      if(icon){
        icon->convert(&this->icon);
      }else{
        this->icon = get_icon(CustomIcon);
      }
    }
  
    
    OSDGLButton(const std::string &id, int x, int y, int w, int h, IconType icon, const Function<void> &cb=Function<void>()):
      id(id),bounds(x,y,w,h),toggable(false),over(false),down(false),
      toggled(false),visible(false),vcb(cb){
      this->icon = get_icon(icon);
    }
    OSDGLButton(const std::string &id, int x, int y, int w, int h, IconType icon, 
                IconType downIcon, const Function<void,bool> &cb=Function<void,bool>(), bool toggled = false):
      id(id),bounds(x,y,w,h),toggable(true),over(false),down(false),
      toggled(toggled),visible(false),bcb(cb){
      this->icon = get_icon(icon);
      this->downIcon = get_icon(downIcon);
    }

    OSDGLButton(const std::string &id, int x, int y, int w, int h, 
                const ImgBase *untoggledIcon, const ImgBase *toggledIcon, 
                const Function<void,bool> &cb=Function<void,bool>(), bool toggled = false):
      id(id),bounds(x,y,w,h),toggable(true),over(false),down(false),
      toggled(toggled),visible(false),bcb(cb){
      if(untoggledIcon){
        untoggledIcon->convert(&icon);
      }else{
        this->icon = get_icon(CustomIcon);
      }
      if(toggledIcon){
        toggledIcon->convert(&downIcon);
      }else{
        if(untoggledIcon){
          untoggledIcon->convert(&downIcon);
        }else{
          downIcon = get_icon(CustomIcon);
        }
      }
    }


    void drawGL(const Size &windowSize){
      if(!visible) return;
      tmImage.setBCI(over*20,down*20,0);
      tmImage.setScaleMode(interpolateLIN);
      const Img8u &im = toggled ? downIcon : icon;
      tmImage.update(&im);
      
      tmImage.draw2D(bounds,windowSize);
    }
    bool update_mouse_move(int x, int y, ICLWidget *parent){
      if(!visible) return false;
      return (over = bounds.contains(x,y)); 
    }

    bool update_mouse_press(int x, int y,ICLWidget *parent){
      if(!visible) return false;
      if(bounds.contains(x,y)){
        down = true;
        if(toggable){
          toggled = !toggled;
          if(bcb) bcb(toggled);//(parent->*bcb)(toggled);
          if(id != "") emit parent->specialButtonToggled(id,toggled);
        }else{
          if(vcb) vcb(); // (parent->*vcb)();
          if(id != "") emit parent->specialButtonClicked(id);
        }
        return true;
      }else{
        return false;
      }
    }
    bool update_mouse_release(int x, int y, ICLWidget *parent){
      if(!visible) return false;
      down = false;
      return bounds.contains(x,y);
    }

    bool event(int x, int y, Event evt, ICLWidget *parent){

      switch(evt){
        case Move: return update_mouse_move(x,y,parent);
        case Press: return update_mouse_press(x,y,parent);
        case Release: return update_mouse_release(x,y,parent);
        case Enter:
        case Leave: 
          visible = evt==Enter;
          return bounds.contains(x,y);
        case Draw: 
          drawGL(Size(parent->width(),parent->height()));
          return false;
      }
      return false;
    }
  };

  // }}}


  struct ImageInfoIndicator : public ThreadedUpdatableWidget{
    // {{{ open
    ImgParams p;
    icl::depth d;
    Point mousePos;
    Size viewPort;
    bool haveImage;
    
    ImageInfoIndicator(QWidget *parent):
      ThreadedUpdatableWidget(parent),haveImage(false){
      d = (icl::depth)(-2);
      setBackgroundRole(QPalette::Window);
      //#if (QT_VERSION >= QT_VERSION_CHECK(4, 5, 0))
      //setAttribute(Qt::WA_TranslucentBackground);
      //#endif
    }

    void update(const ImgParams p, icl::depth d){
      this->p = p;
      this->d = d;
      haveImage = true;
      updateFromOtherThread();
    }
    
    void update(const Size &viewPortSize){
      haveImage = false;
      viewPort = viewPortSize;
      updateFromOtherThread();
    }
    
    void updateMousePos(const Point &mousePos){
      this->mousePos = mousePos;
      updateFromOtherThread();
    }

    inline std::string posstr(){
      return str(mousePos);
    }
    
   inline std::string dstr(){
     switch(d){
#define ICL_INSTANTIATE_DEPTH(D) case depth##D: return #D;
       ICL_INSTANTIATE_ALL_DEPTHS;
#undef ICL_INSTANTIATE_DEPTH
       default: return "?";
     }
   }
   inline std::string rstr(){
     if(p.getROISize() == p.getSize()) return "full";
     return str(p.getROI());
   }

   inline std::string fstr(){
     if(p.getFormat() == formatMatrix){
       return str("mat(")+str(p.getChannels())+")";
     }else{
       return str(p.getFormat());
     }
   }
   virtual void paintEvent(QPaintEvent *e){
     QWidget::paintEvent(e);
     QPainter pa(this);
     pa.setRenderHint(QPainter::Antialiasing);

     pa.setBrush(QColor(210,210,210));
     pa.setPen(QColor(50,50,50));

     pa.drawRect(QRectF(0,0,width(),height()));
     static const char D[] = "-";
     
     if(!haveImage){
       std::string info = "no image, viewport:"+str(viewPort);
       pa.drawText(QRectF(0,0,width(),height()),Qt::AlignCenter,info.c_str());
     }else{
       std::string info = posstr()+" "+dstr()+D+str(p.getSize())+D+rstr()+D+fstr();
       pa.drawText(QRectF(0,0,width(),height()),Qt::AlignCenter,info.c_str());
     }
   }
  };

  // }}}
  
  struct ICLWidget::Data{
        // {{{ open

    Data(ICLWidget *parent):
      parent(parent),channelSelBuf(0),
      qimageConv(0),qimage(0),mutex(QMutex::Recursive),fm(fmHoldAR),fmSave(fmHoldAR),
      rm(rmOff),bciUpdateAuto(false),channelUpdateAuto(false),
      mouseX(-1),mouseY(-1),selChannel(-1),showNoImageWarnings(true),
      outputCap(0),menuOn(true),menuMutex(QMutex::Recursive),menuptr(0),zoomAdjuster(0),
      qic(0),menuEnabled(true),infoTab(0),
      imageInfoIndicatorEnabled(true),infoTabVisible(false),
      selectedTabIndex(0),embeddedZoomMode(false),
      embeddedZoomModeJustEnabled(false),embeddedZoomRect(0),
      useLinInterpolation(false),nextButtonX(2),lastMouseReleaseButton(0),
      defaultViewPort(Size::VGA)
    {
      for(int i=0;i<3;++i){
        bci[i] = 0;
        downMask[i] = 0;
      }
      gridColor[0]=gridColor[1]=gridColor[2]=1;
      gridColor[3]=0.4;
      backgroundColor[0] = backgroundColor[1] = backgroundColor[2] = 0;
    }  
    ~Data(){
      ICL_DELETE(channelSelBuf);
      ICL_DELETE(qimageConv);
      ICL_DELETE(qimage);
      ICL_DELETE(qic);
      ICL_DELETE(embeddedZoomRect);
      
      deleteAllCallbacks();
      // ICL_DELETE(outputCap); this must be done by the parent widget
    }
    
    
    ICLWidget *parent;
    ImgBase *channelSelBuf;
    GLImg image;
    QImageConverter *qimageConv;
    QImage *qimage;
    QMutex mutex;
    fitmode fm;
    fitmode fmSave;
    rangemode rm;
    int bci[3];
    bool *bciUpdateAuto;
    bool *channelUpdateAuto;
    bool downMask[3];
    int mouseX,mouseY;
    int selChannel;
    MouseEvent mouseEvent;
    bool showNoImageWarnings;
    ICLWidget::OutputBufferCapturer *outputCap;
    bool menuOn;

    QMutex menuMutex;    
    GUI menu;
    QWidget *menuptr;
    Rect32f zoomRect;
    ZoomAdjustmentWidgetParent *zoomAdjuster;
    QImageConverter *qic;
    bool menuEnabled;
    QWidget *infoTab;
    HistogrammWidget *histoWidget;
    ImageInfoIndicator *imageInfoIndicator;
    bool imageInfoIndicatorEnabled;
    bool infoTabVisible; // xxx
    bool selectedTabIndex;
    bool embeddedZoomMode;
    bool embeddedZoomModeJustEnabled;
    Rect32f *embeddedZoomRect;
    std::vector<OSDGLButton*> glbuttons;
    bool useLinInterpolation;
    int nextButtonX;
    std::vector<MouseHandler*> callbacks;
    int lastMouseReleaseButton;
    float gridColor[4];
    float backgroundColor[3];
    Point wheelDelta;
    Size defaultViewPort;
    
    bool event(int x, int y, OSDGLButton::Event evt){
      bool any = false;
      for(unsigned int i=0;i<glbuttons.size();++i){
        if(i == 5 && (evt == OSDGLButton::Enter || evt == OSDGLButton::Leave)){
          continue;
        }
        any |= glbuttons[i]->event(x,y,evt,parent);
      }
      return any;
    }

    void deleteAllCallbacks(){
      for(unsigned int i=0;i<callbacks.size();++i){
        delete callbacks[i];
      }
      callbacks.clear();
    }
    
    void updateImageInfoIndicatorGeometry(const QSize &parentSize){
      static const int W = 290;
      imageInfoIndicator->setGeometry(QRect(parentSize.width()-(W+2),parentSize.height()-18,W,18));
    }
    

    void adaptMenuSize(const QSize &parentSize){
      if(!menuptr) return;
      static const int MARGIN = 5;
      static const int TOP_MARGIN = 20;
      int w = parentSize.width()-2*MARGIN;
      int h = parentSize.height()-(MARGIN+TOP_MARGIN);
      
      w = iclMax(menuptr->minimumWidth(),w);
      h = iclMax(menuptr->minimumHeight(),h);
      updateImageInfoIndicatorGeometry(parentSize);
    }
    void pushScaleModeAndChangeToZoom(){
      fmSave = fm;
      fm = fmZoom;
    }
    void popScaleMode(){
      fm = fmSave;
    }
  };

  // }}}

  struct ICLWidget::OutputBufferCapturer{
    // {{{ open

    ICLWidget *parent;
    ICLWidget::Data *data;
    
    bool recording;
    bool paused;
    enum CaptureTarget { SET_IMAGES, FRAME_BUFFER };
    CaptureTarget target;
    Mutex mutex;
    GenericImageOutput imageOutput;
    std::string deviceType;
    std::string deviceInfo;
    int frameSkip;
    int frameIdx;
    FixedConverter *converter;
    ImgBase *convertedBuffer;

  public:
    OutputBufferCapturer(ICLWidget *parent, ICLWidget::Data *data):
      parent(parent),data(data),target(SET_IMAGES),
      frameSkip(0),frameIdx(0),converter(0),convertedBuffer(0){}
    
    ~OutputBufferCapturer(){
      ICL_DELETE(converter);
      ICL_DELETE(convertedBuffer);
    }

    void ensureDirExists(const QString &dirName){
      if(!dirName.length()){
        return;
      }
      if(dirName[0] == '/'){
        QDir("/").mkpath(dirName);
      }else{
        QDir("./").mkpath(dirName);
      }
    }
    bool startRecording(CaptureTarget t, const std::string &device, const std::string &params, int frameSkip,
                        bool forceParams, const Size &dstSize, icl::format dstFmt,  icl::depth dstDepth){
      Mutex::Locker l(mutex);
    
      ICL_DELETE(converter);
      if(forceParams){
        converter = new FixedConverter(ImgParams(dstSize,dstFmt),dstDepth);
      }

      if(device == "file"){
        ensureDirExists(File(params).getDir().c_str());        
      }else if(device == "video"){
        std::vector<std::string> ts = tok(params,",");
        if(!ts.size()) throw ICLException(str(__FUNCTION__)+": Error 12345 (this should not happen)");
        File f(ts.front());
        if(f.exists()){
          QMessageBox::StandardButton b = QMessageBox::question(0,"file exists",QString("given input file\n")+
                                                                ts.front().c_str()+"\nexits! overwrite?",
                                                                QMessageBox::Yes | QMessageBox::No);
          if(b == QMessageBox::Yes){
            f.erase();
          }else{
            return false;
          }
        }
        ensureDirExists(File(ts.front()).getDir().c_str());
      }
      target = t;
      this->frameSkip = frameSkip;
      std::string initError;
      
      try{
        imageOutput.init(device,params);
      }catch(std::exception &ex){
        initError = ex.what();
      }
      if(initError.length() || imageOutput.isNull()){
        std::string err = initError.length() ? initError : "unknown error";
        QMessageBox::critical(0,"error",("Unable to create image output device: (" + initError + ")").c_str());
        return false;
      }
      deviceType = deviceType;
      deviceInfo = params;

      recording = true;
      paused = false;
      return true;
    }
    bool setPaused(bool val){
      Mutex::Locker l(mutex);
      paused = val;
      return paused;
    }
    bool stopRecording(){
      Mutex::Locker l(mutex);
      recording = false;
      return recording;
    }
    
    void captureImageHook(){
      Mutex::Locker l(mutex);
      if(!recording || paused || (target != SET_IMAGES) ) return;
      ICLASSERT_RETURN(!imageOutput.isNull());
      if(frameIdx < frameSkip){
        frameIdx++;
        return;
      }else{
        frameIdx = 0;
      }
      try{
        SmartPtr<const ImgBase> image(data->image.extractImage(),false);
        
        if(converter){
          converter->apply(image.get(),&convertedBuffer);
          image = SmartPtr<ImgBase>(convertedBuffer,false);
        }
        imageOutput.send(image.get());
      }catch(ICLException &ex){
        ERROR_LOG("unable to capture current image:" << ex.what());
      }
    }

    void captureFrameBufferHook(){
      Mutex::Locker l(mutex);

      if(!recording || paused || (target != FRAME_BUFFER)) return;
      ICLASSERT_RETURN(!imageOutput.isNull());

      if(frameIdx < frameSkip){
        frameIdx++;
        return;
      }else{
        frameIdx = 0;
      }
      try{
        const ImgBase *fb = &parent->grabFrameBufferICL();
        if(converter){
          converter->apply(fb,&convertedBuffer);
          fb = convertedBuffer;
        }
        imageOutput.send(fb);
      }catch(ICLException &ex){
        ERROR_LOG("unable to capture frame buffer:" << ex.what());
      }
    }
  };

  // }}}
  
  static Rect computeRect(const Size &imageSize, const Size &widgetSize, ICLWidget::fitmode mode,const Rect32f &relZoomRect=Rect32f::null){
    // {{{ open

    int iImageW = imageSize.width;
    int iImageH = imageSize.height;
    int iW = widgetSize.width;
    int iH = widgetSize.height;
    
    switch(mode){
      case ICLWidget::fmNoScale:
        // set up the image rect to be centeed
        return Rect((iW -iImageW)/2,(iH -iImageH)/2,iImageW,iImageH); 
        break;
      case ICLWidget::fmHoldAR:{
        // check if the image is more "widescreen" as the widget or not
        // and adapt image-rect
        float fWidgetAR = (float)iW/(float)iH;
        float fImageAR = (float)iImageW/(float)iImageH;
        if(fImageAR >= fWidgetAR){ //Image is more "widescreen" then the widget
          float fScaleFactor = (float)iW/(float)iImageW;
          return Rect(0,(iH-(int)floor(iImageH*fScaleFactor))/2,
                      (int)floor(iImageW*fScaleFactor),(int)floor(iImageH*fScaleFactor));
        }else{
          float fScaleFactor = (float)iH/(float)iImageH;
          return Rect((iW-(int)floor(iImageW*fScaleFactor))/2,0,
                      (int)floor(iImageW*fScaleFactor),(int)floor(iImageH*fScaleFactor));
        }
        break;
      }
      case ICLWidget::fmFit:
        // the image is force to fit into the widget
        return Rect(0,0,iW,iH);
        break;
      case ICLWidget::fmZoom:{
        const Rect32f &rel = relZoomRect;
        float x = (rel.x/rel.width)*widgetSize.width;
        float y = (rel.y/rel.height)*widgetSize.height;
        
        float relRestX = 1.0-rel.right();
        float relRestY = 1.0-rel.bottom();
        
        float restX = (relRestX/rel.width)*widgetSize.width; 
        float restY = (relRestY/rel.height)*widgetSize.height;
          
        return Rect(round(-x),round(-y),round(x+widgetSize.width+restX),round(y+widgetSize.height+restY));
        break;
      }
    }
    return Rect(0,0,iW,iH);
  }

  // }}}

  static Rect32f fixRectAR(const Rect32f &r, ICLWidget *w){
    // {{{ open
    // Adapt the rects aspect ratio to current image size
    Size widgetSize = w->getSize();
    Size imageSize = w->getImageSize(true);
    float fracOrig = r.width*r.height;
    float imAR = float(imageSize.width)/float(imageSize.height);
    float widAR = float(widgetSize.width)/float(widgetSize.height);
    
    float relW = sqrt((widAR/imAR) * fracOrig);
    float relH = fracOrig/relW;
    
    Point32f cOrig = r.center();
    return Rect32f(cOrig.x-relW/2,cOrig.y-relH/2,relW,relH);
  }
  // }}}
  
  struct ZoomAdjustmentWidget : public QWidget{
    // {{{ open

    Rect32f &r;
    bool downMask[3];
    
    enum Edge { LEFT=0, TOP=1, RIGHT=2, BOTTOM=3 };
    enum LeftDownMode { DRAG, CREATE, NONE };
    bool edgesHovered[4];
    bool edgesDragged[4];
    LeftDownMode mode;
    bool dragAll;

    Point32f dragAllOffs;
    Point32f currPos;
    
    QWidget *parentICLWidget;
    
    ZoomAdjustmentWidget(QWidget *parent, Rect32f &r, QWidget *parentICLWidget):
      QWidget(parent),r(r),mode(NONE),dragAll(false),parentICLWidget(parentICLWidget){
      downMask[0]=downMask[1]=downMask[2]=false;
      //      r = Rect32f(0,0,width(),height());
      for(int i=0;i<4;++i){
        edgesHovered[i] = edgesDragged[i] = false;
      }
      setMouseTracking(true);
    }

    float xr2a(float rel){
      return rel*width();
    }
    float yr2a(float rel){
      return rel*height();
    }
    float xa2r(float abs){
      return abs/width();
    }
    float ya2r(float abs){
      return abs/height();
    }
    QPointF r2a(const QPointF &rel){
      return QPointF(xr2a(rel.x()),yr2a(rel.y()));
    }
    QPointF a2r(const QPointF &abs){
      return QPointF(xa2r(abs.x()),ya2r(abs.y()));
    }

    bool hit(Edge e,const QPointF &p){
      static const float D = 0.02;
      switch(e){
        case LEFT: 
          if(!Range32f(r.x-D,r.x+D).contains(p.x())) return false;
          if(!Range32f(r.y-D,r.bottom()+D).contains(p.y())) return false;
          return true;
        case RIGHT:
          if(!Range32f(r.right()-D,r.right()+D).contains(p.x())) return false;
          if(!Range32f(r.y-D,r.bottom()+D).contains(p.y())) return false;
          return true;
        case TOP: 
          if(!Range32f(r.y-D,r.y+D).contains(p.y())) return false;
          if(!Range32f(r.x-D,r.right()+D).contains(p.x())) return false;
          return true;
        case BOTTOM: 
          if(!Range32f(r.bottom()-D,r.bottom()+D).contains(p.y())) return false;
          if(!Range32f(r.x-D,r.right()+D).contains(p.x())) return false;
          return true;
      }
      return false;
    }

    bool hitAny(const QPointF &p){
      return hit(LEFT,p) || hit(TOP,p) || hit(RIGHT,p) || hit(BOTTOM,p);
    }
    bool dragAny(){
      return dragAll || edgesDragged[LEFT] || edgesDragged[TOP] || edgesDragged[RIGHT] || edgesDragged[BOTTOM];
    }

    void normalize(){
      r = r.normalized();
      if(r.x < 0) r.x = 0;
      if(r.y < 0) r.y = 0;
      if(r.right() > 1) r.width = 1.0-r.x;
      if(r.bottom() > 1) r.height = 1.0-r.y;
    }
   
    QPen pen(Edge e){
      return QPen((dragAll||edgesDragged[e])?QColor(255,0,0):QColor(50,50,50),edgesHovered[e] ? 2.5 : 0.5);
    }
    
    virtual void paintEvent(QPaintEvent *e){
      QWidget::paintEvent(e);
      QPainter p(this);
      if(dragAny() || r.contains(currPos.x,currPos.y)){
        p.setPen(Qt::NoPen);
        p.setBrush(QBrush(QColor(255,0,0),Qt::BDiagPattern));
        p.drawRect(QRectF(width()*r.x,height()*r.y,width()*r.width,height()*r.height));
      }
      
      p.setPen(QColor(50,50,50));
      p.setBrush(Qt::NoBrush);
      p.setRenderHint(QPainter::Antialiasing);
      p.drawRect(QRectF(0,0,width(),height()));
      
      float sx = width();
      float sy = height();
      p.setPen(QColor(50,50,50));

      p.setPen(pen(LEFT));
      p.drawLine(QPointF(sx*r.x,sy*r.y),QPointF(sx*r.x,sy*r.bottom()));

      p.setPen(pen(TOP));
      p.drawLine(QPointF(sx*r.x,sy*r.y),QPointF(sx*r.right(),sy*r.y));

      p.setPen(pen(RIGHT));
      p.drawLine(QPointF(sx*r.right(),sy*r.y),QPointF(sx*r.right(),sy*r.bottom()));

      p.setPen(pen(BOTTOM));
      p.drawLine(QPointF(sx*r.x,sy*r.bottom()),QPointF(sx*r.right(),sy*r.bottom()));
    }

    bool &down(Qt::MouseButton b){
      static bool _ = false;
      switch(b){
        case Qt::LeftButton: return downMask[0];
        case Qt::MidButton: return downMask[1];
        case Qt::RightButton: return downMask[2];
        default: return _;
      }
    }
    
    /// LEFT drag and drop corners / and redraw new rect
    void leftDown(const QPointF &p){
      if(hitAny(p)){
        for(Edge e=LEFT;e<=BOTTOM;((int&)e)++){
          if(hit(e,p)){
            edgesHovered[e] = edgesDragged[e] = true;
          }else{
            edgesHovered[e] = edgesDragged[e] = false;
          }
        }
        mode = DRAG;
      }else{
        r = Rect32f(p.x(),p.y(),0.0001,0.0001);
        mode = CREATE;
        parentICLWidget->update();
      }
    }
    void leftUp(const QPointF &p){
      for(Edge e=LEFT;e<=BOTTOM;((int&)e)++){
        edgesDragged[e] = false;
      }
      mode = NONE;
      r = fixRectAR(r,reinterpret_cast<ICLWidget*>(parentICLWidget));
      parentICLWidget->update();
    }
    
    void leftDrag(const QPointF &p){
      float left=r.x;
      float right=r.right();
      float top = r.y;
      float bottom = r.bottom();
      if(mode == DRAG){
        if(edgesDragged[LEFT])left = p.x();
        else if(edgesDragged[RIGHT])right = p.x();
        if(edgesDragged[TOP])top = p.y();
        else if(edgesDragged[BOTTOM])bottom = p.y();
        r = Rect32f(left,top,right-left,bottom-top);
      }else if(mode == CREATE){
        r = Rect32f(r.x,r.y,p.x()-r.x, p.y()-r.y);
      }else{
        ERROR_LOG("this should not happen!");
      }
      normalize();
      parentICLWidget->update();
    }
    
    void move(const QPointF &p){
      currPos = Point32f(p.x(),p.y());
      for(Edge e=LEFT;e<=BOTTOM;((int&)e)++){
        edgesHovered[e] = hit(e,p);
      }
    }
    
    void rightDown(const QPointF &p){
      if(r.contains(p.x(),p.y())){
        dragAll = true;
        dragAllOffs = r.ul()-Point32f(p.x(),p.y());
      }
    }
    void rightUp(const QPointF &p){
      dragAll = false;
    }
    void rightDrag(const QPointF &p){
      if(dragAll){
        r.x = p.x()+dragAllOffs.x;
        r.y = p.y()+dragAllOffs.y;
        parentICLWidget->update();
      }
    }
    
#if QT_VERSION >= 0x040400
#define MOUSE_EVENT_POS e->posF()
#else
#define MOUSE_EVENT_POS QPointF(e->pos().x(),e->pos().y())
#endif
    virtual void mousePressEvent(QMouseEvent *e){
      down(e->button()) = true;
      if(e->button() == Qt::LeftButton){
        leftDown(a2r(MOUSE_EVENT_POS));
      }else if(e->button() == Qt::RightButton){
        rightDown(a2r(MOUSE_EVENT_POS));
      }
      update();
    }
    
    virtual void mouseReleaseEvent(QMouseEvent *e){
      down(e->button()) = false;
      if(e->button() == Qt::LeftButton){
        leftUp(a2r(MOUSE_EVENT_POS));
      }else if(e->button() == Qt::RightButton){
        rightUp(a2r(MOUSE_EVENT_POS));
      }      
      update();
    }
    
    virtual void mouseMoveEvent(QMouseEvent *e){
      if(downMask[0]){
        leftDrag(a2r(MOUSE_EVENT_POS));
      }else if(downMask[2]){
        rightDrag(a2r(MOUSE_EVENT_POS));
      }else{
        move(a2r(MOUSE_EVENT_POS));
      }
      update();
    }
  };

  // }}}

  struct ZoomAdjustmentWidgetParent : public QWidget{
    // {{{ open

    Size imageSize;
    ZoomAdjustmentWidget *aw;
    ZoomAdjustmentWidgetParent(const Size &imageSize, QWidget *parent, Rect32f &r, QWidget *parentICLWidget):
      QWidget(parent),imageSize(imageSize==Size::null ? Size(100,100): imageSize){
      aw = new ZoomAdjustmentWidget(this,r,parentICLWidget);
    }
    void updateAWSize(){
      Rect r = computeRect(imageSize,Size(width(),height()), ICLWidget::fmHoldAR);
      aw->setGeometry(QRect(r.x,r.y,r.width,r.height));
      update();
    }
    
    void resizeEvent(QResizeEvent *e){
      updateAWSize();
    }
    void updateImageSize(const Size &imageSize){
      this->imageSize = imageSize == Size::null ? Size(100,100) : imageSize;
      updateAWSize();
    }    
  };

  // }}}

  static void create_menu(ICLWidget *widget,ICLWidget::Data *data){
    // {{{ open

    QMutexLocker locker(&data->menuMutex);

    // OK, we need to extract default values for all gui elements if gui is already defined!
    data->menu = GUI("tab(bci,scale,channel,capture,grid,info,license,help)[@handle=root@minsize=5x7]",widget);

    GUI bciGUI("vbox");

    std::string bcis[3]={"custom,","off,","auto"};
    bcis[((int)data->rm)-1] = str("!")+bcis[((int)data->rm)-1];
    bool bciAuto = data->bciUpdateAuto && *data->bciUpdateAuto;
    bciGUI << ( GUI("hbox")
                << str("combo(")+bcis[0]+bcis[1]+bcis[2]+")[@label=bci-mode@handle=bci-mode@out=bci-mode-out]"
                << str("togglebutton(manual,")+(bciAuto?"!":"")+"auto)[@label=update mode@out=bci-update-mode]"
              )
           << str("slider(-255,255,")+str(data->bci[0])+")[@handle=brightness@label=brightness@out=_2]"
           << str("slider(-255,255,")+str(data->bci[1])+")[@handle=contrast@label=contrast@out=_1]"
           << str("slider(-255,255,")+str(data->bci[2])+")[@handle=intensity@label=intensity@out=_0]";
    
    GUI scaleGUI("vbox");
    std::string em[4]={"no scale,","hold aspect ratio,","force fit,","zoom"};
    em[data->fm] = str("!")+em[data->fm];
    scaleGUI << str("combo(")+em[0]+em[1]+em[2]+em[3]+")[@maxsize=100x2@handle=fit-mode@label=scale mode@out=_3]";
    scaleGUI << "hbox[@handle=scale-widget@minsize=4x3]";

    GUI channelGUI("vbox");
    channelGUI << "combo(all,channel #0,channel #1,channel #2,channel #3)[@maxsize=100x2@handle=channel@label=visualized channel@out=_4]"
               << "togglebutton(manual,auto)[@maxsize=100x2@label=update mode@out=channel-update-mode]";

    GUI captureGUI("vbox");
     
    captureGUI << ( GUI("hbox")
                    << ( GUI("hbox[@label=single shot]") 
                         << "button(image)[@handle=cap-image]"
                         << "button(frame buffer)[@handle=cap-fb]"
                       )
                    << "combo(image.pnm,image_TIME.pnm,ask me)[@label=filename@handle=cap-filename@out=_5]"
                  );
                  
    
    bool autoCapFB = data->outputCap && data->outputCap->target==ICLWidget::OutputBufferCapturer::FRAME_BUFFER;
    int autoCapFS = data->outputCap ? (data->outputCap->frameSkip) : 0;
    bool autoCapRec = data->outputCap && data->outputCap->recording;
    bool autoCapPau = data->outputCap && data->outputCap->paused;

    std::string FILE = "file";
    std::string VIDEO = "video";
    std::string XCFP = "xcfp";
    std::string SM = "sm";
    if(data->outputCap){
      if(data->outputCap->deviceType == "file") FILE = "!"+FILE;
      else if(data->outputCap->deviceType == "video") VIDEO = "!"+VIDEO;
      else if(data->outputCap->deviceType == "xcfp") XCFP = "!"+XCFP;
      else if(data->outputCap->deviceType == "sm") SM = "!"+SM;
    }
    std::string autoCapFP = data->outputCap ? data->outputCap->deviceInfo : "captured/image_####.ppm";
    
    GUI autoCapGUI("vbox[@label=automatic]");
    autoCapGUI << ( GUI("hbox")
                    << str("combo(image,")+(autoCapFB?"!":"")+"frame buffer)[@label=mode@handle=auto-cap-mode@out=_6]"
                    << str("spinner(0,100,")+str(autoCapFS)+")[@label=frame skip@handle=auto-cap-frameskip@out=_10]"
                    << str("combo(")+FILE+","+VIDEO+","+XCFP+","+SM+")[@label=dest.@handle=auto-cap-device]"
                    << str("string(")+autoCapFP+",200)[@label=output params@handle=auto-cap-filepattern@out=_7]"
                  )
               << ( GUI("hbox")
                    << "checkbox(force params,unchecked)[@out=auto-cap-force]"
                    << "combo(160x120,320x240,!640x480,800x600,1024x768,1200x800,1600x1200,1280x7200,1920x1080)"
                       "[@label=size@handle=auto-cap-size]"
                    << "combo(gray,!rgb,hls,yuv,lab)[@label=color@handle=auto-cap-format]"
                    << "combo(depth8u,depth16s,depth32s,depth32f,depth64f)[@label=depth@handle=auto-cap-depth]"
                  )
               << ( GUI("hbox")
                    << str("togglebutton(record,")+(autoCapRec?"!":"")+"record)[@handle=auto-cap-record@out=_8]"
                    << str("togglebutton(pause,")+(autoCapPau?"!":"")+"pause)[@handle=auto-cap-pause@out=_9]"
                    << "button(stop)[@handle=auto-cap-stop]"
                  );
    captureGUI << autoCapGUI;    
    
    GUI extraGUI("vbox");

    extraGUI << (GUI("hbox[@label=background color]")
                 << "button(select color)[@handle=select-bg-color]"
                 << "button(black)[@handle=bg-black]"
                 << "button(white)[@handle=bg-white]"
                 << "button(gray)[@handle=bg-gray]" )
                 << (GUI("hbox") 
                 << "togglebutton(off,on)[@handle=grid-on@label=show grid]"
        << "slider(0,255,100)[@label=grid alpha@handle=grid-alpha]")
             << (GUI("hbox[@label=grid color]")
                 << "button(select color)[@handle=select-grid-color]"
                 << "button(black)[@handle=grid-black]"
                 << "button(white)[@handle=grid-white]"
                 << "button(gray)[@handle=grid-gray]" );
    

    GUI infoGUI("vsplit[@handle=info-tab]");
    infoGUI << ( GUI("hbox")
                 << "combo(all ,channel #0, channel #1,channel #2, channel #3)[@handle=histo-channel@out=_15]"
                 << "togglebutton(median,median)[@handle=median@out=median-on]"
                 << "togglebutton(log,log)[@handle=log@out=log-on]"
                 << "togglebutton(blur,blur)[@handle=blur@out=blur-on]"
                 << "togglebutton(fill,fill)[@handle=fill@out=fill-on]"
               )
            <<  ( GUI("hsplit")
                  << "hbox[@label=histogramm@handle=histo-box@minsize=12x10]"
                  << "label[@label=params@handle=image-info-label]"
                );

    GUI licGUI("vbox[@handle=lic]");

    GUI helpGUI("vbox[@handle=help]");
    
    data->menu << bciGUI << scaleGUI << channelGUI << captureGUI << extraGUI << infoGUI << licGUI << helpGUI;
    
    data->menu.create();


    (*data->menu.getValue<ComboHandle>("auto-cap-mode"))->setToolTip("== Choose What to Capture =="
                                                                     "\n"
                                                                     "= image =\n"
                                                                     "Captures each image that is set via\n"
                                                                     "setImage(..). In this case, only the\n"
                                                                     "image in its real unscaled size is\n" 
                                                                     "captured\n"
                                                                     "\n"
                                                                     "= frame buffer=\n"
                                                                     "Captures each framebuffer, that is\n"
                                                                     "displayed. In this case, the whole\n"
                                                                     "Widget content is captured (including\n"
                                                                     "2D and 3D overlay). The image will\n"
                                                                     "be captured in it's visualized (scaled)\n"
                                                                     "size. If you minimize the window, the\n"
                                                                     "resulting images might become black");

    (*data->menu.getValue<ComboHandle>("auto-cap-device"))->setToolTip("== Choose Output Device Type ==\n"
                                                                       "\n"
                                                                       "= file =\n"
                                                                       "Captures each image as file to disk.\n"
                                                                       "\n"
                                                                       "= video =\n"
                                                                       "Create a video output stream and encodes\n"
                                                                       "each image online. (only possible if OpenCV2\n"
                                                                       "is available)\n"
                                                                       "\n"
                                                                       "= xcfp =\n"
                                                                       "Creates an xcf-publisher to publish images\n"
                                                                       "via network (only possible if XCF is \n"
                                                                       "is available)\n"
                                                                       "\n"
                                                                       "= sm =\n"
                                                                       "Creates a SharedMemoryPublisher, that\n"
                                                                       "publishs the image via QShareMemory"
                                                                       );
    (*data->menu.getValue<StringHandle>("auto-cap-filepattern"))->setToolTip("== Specify Parameters for Capturing\n"
                                                                             "The allowed parameters depend on the\n"
                                                                             "used device type.\n"
                                                                             "\n"
                                                                             "= file =\n"
                                                                             "Filepattern like ./images/image_####.ppm\n"
                                                                             "\n"
                                                                             "= video =\n"
                                                                             "Syntax FILENAME[,CODEC[,SIZE[,FPS]]]\n"
                                                                             "like foo.avi,DIVX,24\n"
                                                                             "or simply bla.avi\n"
                                                                             "possible codes are: \n"
                                                                             " * PIM1 (for mpeg 1)\n"
                                                                             " * MJPG (for motion jepg)\n"
                                                                             " * MP42 (for mpeg 4.2)\n"
                                                                             " * DIV3 (for mpeg 4.3)\n"
                                                                             " * DIVX (for mpeg 4)\n"
                                                                             " * U263 (for H263 codec)\n"
                                                                             " * I263 (for H263I codec)\n"
                                                                             " * FLV1 (for FLV1 code)\n"
                                                                             "\n"
                                                                             "= xcfp =\n"
                                                                             "XCF-stream name like 'stream'\n"
                                                                             "\n"
                                                                             "= sm =\n"
                                                                             "Shared memory segment name like MySMEM");


    data->menuptr = data->menu.getRootWidget();
    data->menuptr->setParent(widget);
    // xxx new layout test
    if(!widget->layout()){
      widget->setLayout(new QGridLayout);
      widget->layout()->setContentsMargins(5,22,5,5);
    }
    widget->layout()->addWidget(data->menuptr);


    /// xxx new bg tset
    data->menuptr->setAutoFillBackground(true);
    //data->menuptr->setStyleSheet("QWidget { background : rgba(200,200,200,10) }");
    
    
    data->bciUpdateAuto = &data->menu.getValue<bool>("bci-update-mode");
    data->channelUpdateAuto = &data->menu.getValue<bool>("channel-update-mode");

    data->infoTab = *data->menu.getValue<SplitterHandle>("info-tab");
    
    data->histoWidget = new HistogrammWidget(*data->menu.getValue<BoxHandle>("histo-box"));
    data->menu.getValue<BoxHandle>("histo-box").add(data->histoWidget);
    
    QObject::connect(*data->menu.getValue<ComboHandle>("bci-mode"),SIGNAL(currentIndexChanged(int)),widget,SLOT(bciModeChanged(int)));
    QObject::connect(*data->menu.getValue<ComboHandle>("fit-mode"),SIGNAL(currentIndexChanged(int)),widget,SLOT(scaleModeChanged(int)));
    QObject::connect(*data->menu.getValue<ComboHandle>("channel"),SIGNAL(currentIndexChanged(int)),widget,SLOT(currentChannelChanged(int)));

    QObject::connect(*data->menu.getValue<SliderHandle>("brightness"),SIGNAL(valueChanged(int)),widget,SLOT(brightnessChanged(int)));
    QObject::connect(*data->menu.getValue<SliderHandle>("contrast"),SIGNAL(valueChanged(int)),widget,SLOT(contrastChanged(int)));
    QObject::connect(*data->menu.getValue<SliderHandle>("intensity"),SIGNAL(valueChanged(int)),widget,SLOT(intensityChanged(int)));
    
    data->zoomAdjuster = new ZoomAdjustmentWidgetParent(Size::null,0,data->zoomRect,widget);
    data->menu.getValue<BoxHandle>("scale-widget").add(data->zoomAdjuster);

    QObject::connect(*data->menu.getValue<ButtonHandle>("cap-image"),SIGNAL(clicked()),widget,SLOT(captureCurrentImage()));
    QObject::connect(*data->menu.getValue<ButtonHandle>("cap-fb"),SIGNAL(clicked()),widget,SLOT(captureCurrentFrameBuffer()));
    

    QObject::connect(*data->menu.getValue<ButtonHandle>("auto-cap-record"),SIGNAL(toggled(bool)),widget,SLOT(recordButtonToggled(bool)));
    QObject::connect(*data->menu.getValue<ButtonHandle>("auto-cap-pause"),SIGNAL(toggled(bool)),widget,SLOT(pauseButtonToggled(bool)));
    QObject::connect(*data->menu.getValue<ButtonHandle>("auto-cap-stop"),SIGNAL(clicked()),widget,SLOT(stopButtonClicked()));
    QObject::connect(*data->menu.getValue<ComboHandle>("auto-cap-mode"),SIGNAL(currentIndexChanged(int)),widget,SLOT(stopButtonClicked()));
    QObject::connect(*data->menu.getValue<SpinnerHandle>("auto-cap-frameskip"),SIGNAL(valueChanged(int)),widget,SLOT(skipFramesChanged(int)));
    
    QObject::connect(*data->menu.getValue<TabHandle>("root"),SIGNAL(currentChanged(int)),widget,SLOT(menuTabChanged(int)));
    
    QObject::connect(*data->menu.getValue<ButtonHandle>("fill"),SIGNAL(toggled(bool)),widget,SLOT(histoPanelParamChanged()));
    QObject::connect(*data->menu.getValue<ButtonHandle>("blur"),SIGNAL(toggled(bool)),widget,SLOT(histoPanelParamChanged()));
    QObject::connect(*data->menu.getValue<ButtonHandle>("median"),SIGNAL(toggled(bool)),widget,SLOT(histoPanelParamChanged()));
    QObject::connect(*data->menu.getValue<ButtonHandle>("log"),SIGNAL(toggled(bool)),widget,SLOT(histoPanelParamChanged()));
    QObject::connect(*data->menu.getValue<ComboHandle>("histo-channel"),SIGNAL(currentIndexChanged(int)),widget,SLOT(histoPanelParamChanged()));

    
    QObject::connect(*data->menu.getValue<ButtonHandle>("grid-on"),SIGNAL(toggled(bool)),widget,SLOT(setShowPixelGridEnabled(bool)));
    QObject::connect(*data->menu.getValue<ButtonHandle>("select-grid-color"),SIGNAL(clicked()),widget,SLOT(showGridColorDialog()));
    QObject::connect(*data->menu.getValue<ButtonHandle>("grid-black"),SIGNAL(clicked()),widget,SLOT(setGridBlack()));
    QObject::connect(*data->menu.getValue<ButtonHandle>("grid-white"),SIGNAL(clicked()),widget,SLOT(setGridWhite()));
    QObject::connect(*data->menu.getValue<ButtonHandle>("grid-gray"),SIGNAL(clicked()),widget,SLOT(setGridGray()));
    QObject::connect(*data->menu.getValue<SliderHandle>("grid-alpha"),SIGNAL(sliderMoved(int)),widget,SLOT(setGridAlpha(int)));

    QObject::connect(*data->menu.getValue<ButtonHandle>("select-bg-color"),SIGNAL(clicked()),widget,SLOT(showBackgroundColorDialog()));
    QObject::connect(*data->menu.getValue<ButtonHandle>("bg-black"),SIGNAL(clicked()),widget,SLOT(setBackgroundBlack()));
    QObject::connect(*data->menu.getValue<ButtonHandle>("bg-white"),SIGNAL(clicked()),widget,SLOT(setBackgroundWhite()));
    QObject::connect(*data->menu.getValue<ButtonHandle>("bg-gray"),SIGNAL(clicked()),widget,SLOT(setBackgroundGray()));
                 
            
    { //ugly :-)    
      // license widget
      BoxHandle &h = data->menu.getValue<BoxHandle>("lic");
      QTextEdit *lic = new QTextEdit(*h);
      lic->setReadOnly(true);
      lic->setWordWrapMode(QTextOption::QTextOption::NoWrap);
      lic->setText(pagetlic().c_str());
      h.add(lic,"License Information");
    }{
      BoxHandle &h = data->menu.getValue<BoxHandle>("help");
      QTextEdit *help = new QTextEdit(*h);
      help->setReadOnly(true);
      help->setWordWrapMode(QTextOption::QTextOption::NoWrap);
      std::string text = pagethelp();
      help->setText(text.length() ? text.c_str() : 
                   "no help information available\n"
                   "applications can set a help text\n"
                   "using pasethelp(text)");
      h.add(help,"Help Information");
    }
  }

  // }}}
  
  void update_data(const Size &newImageSize, ICLWidget::Data *data){
    // {{{ open

    /// XXX
    data->menuMutex.lock();
    if(data->menuptr){
      // TODO this cannot be done at runtime -> so the zoom adjusters state must be tracked in 
      // another way ...
      if(data->zoomAdjuster->isVisible()){
        data->zoomAdjuster->updateImageSize(newImageSize);
      }
    }
    data->menuMutex.unlock();
  }

  // }}}

  // ------------ ICLWidget ------------------------------

#define GL_BUTTON_Y 2
#define GL_BUTTON_W 20
#define GL_BUTTON_H 20
#define GL_BUTTON_SPACE 2
#define GL_BUTTON_X_INC (GL_BUTTON_W+GL_BUTTON_SPACE)

  ICLWidget::ICLWidget(QWidget *parent) : 
    // {{{ open

    m_data(new ICLWidget::Data(this)){
    
    setMouseTracking(true);
    setWindowIcon(IconFactory::create_icl_window_icon_as_qicon());
    
    static const int y = GL_BUTTON_Y, w = GL_BUTTON_W, h = GL_BUTTON_H;
    int &x = m_data->nextButtonX;
    m_data->glbuttons.push_back(new OSDGLButton("",x,y,w,h,OSDGLButton::Tool,
                                                icl::function(this,&ICLWidget::showHideMenu)));
    x+=GL_BUTTON_X_INC;
    m_data->glbuttons.push_back(new OSDGLButton("",x,y,w,h,OSDGLButton::Unlock,OSDGLButton::Lock,
                                                icl::function(this,&ICLWidget::setMenuEmbedded),true));
    x+=GL_BUTTON_X_INC;
    m_data->glbuttons.push_back(new OSDGLButton("",x,y,w,h,OSDGLButton::NNInter,OSDGLButton::LINInter,
                                                icl::function(this,&ICLWidget::setLinInterpolationEnabled),false));
    x+=GL_BUTTON_X_INC;
    m_data->glbuttons.push_back(new OSDGLButton("",x,y,w,h,OSDGLButton::Zoom,OSDGLButton::RedZoom,
                                                icl::function(this,&ICLWidget::setEmbeddedZoomModeEnabled),false));
    x+=GL_BUTTON_X_INC;
    m_data->glbuttons.push_back(new OSDGLButton("",x,y,w,h,OSDGLButton::RangeNormal,OSDGLButton::RangeScaled,
                                                icl::function(this,&ICLWidget::setRangeModeNormalOrScaled),false));
    x+=GL_BUTTON_X_INC;
    m_data->glbuttons.push_back(new OSDGLButton("",x,y,w,h,OSDGLButton::RedCam,
                                                icl::function(this,&ICLWidget::stopButtonClicked)));
    x+=GL_BUTTON_X_INC;

    m_data->imageInfoIndicator = new ImageInfoIndicator(this);
    m_data->imageInfoIndicator->update(m_data->defaultViewPort);
  }

  // }}}
  
  ICLWidget::~ICLWidget(){
    // {{{ open

    ICL_DELETE(m_data->outputCap);// just because of the classes definition order 
    delete m_data; 
  }

  // }}}


  void ICLWidget::addSpecialToggleButton(const std::string &id, 
                                         const ImgBase* untoggledIcon, 
                                         const ImgBase *toggledIcon, 
                                         bool initiallyToggled, 
                                         const Function<void,bool> &cb){
    // {{{ open

    static const int y = GL_BUTTON_Y, w = GL_BUTTON_W, h = GL_BUTTON_H;
    int &x = m_data->nextButtonX;
    m_data->glbuttons.push_back(new OSDGLButton(id,x,y,w,h,untoggledIcon,toggledIcon,cb,initiallyToggled));
    
    x+=GL_BUTTON_X_INC;
    
    
  }

  // }}}
  void ICLWidget::addSpecialButton(const std::string &id, 
                                   const ImgBase* icon, 
                                   const Function<void> &cb){
    // {{{ open

    static const int y = GL_BUTTON_Y, w = GL_BUTTON_W, h = GL_BUTTON_H;
    int &x = m_data->nextButtonX;
    m_data->glbuttons.push_back(new OSDGLButton(id,x,y,w,h,icon,cb));
    
    x+=GL_BUTTON_X_INC;
  }

  // }}}
  
  void ICLWidget::removeSpecialButton(const std::string &id){
    // {{{ open

    DEBUG_LOG("removins special buttons is not yet implemented!");
  }

  // }}}

  void ICLWidget::setEmbeddedZoomModeEnabled(bool enabled){
    // {{{ open

    m_data->embeddedZoomMode = enabled;
    if(!enabled){
      ICL_DELETE(m_data->embeddedZoomRect);
      m_data->zoomRect = Rect32f(0,0,getImageSize().width,getImageSize().height);
      m_data->popScaleMode();
      update();
    }else{
      m_data->embeddedZoomModeJustEnabled = true;
    }
    
  }

  // }}} 

  void ICLWidget::setLinInterpolationEnabled(bool enabled){
    // {{{ open
    m_data->useLinInterpolation = enabled;
  }
  // }}}

  void ICLWidget::bciModeChanged(int modeIdx){
    // {{{ open

    switch(modeIdx){
      case 0: m_data->rm = rmOn; break;
      case 1: m_data->rm = rmOff; break;
      case 2: m_data->rm = rmAuto; break;
      default: ERROR_LOG("invalid range mode index");
    }
    if(*m_data->bciUpdateAuto){
      rebufferImageInternal();
    }
  }

  // }}}

  void ICLWidget::brightnessChanged(int val){
    // {{{ open

    m_data->bci[0] = val;
    if(*m_data->bciUpdateAuto){
      rebufferImageInternal();
    }
  }

  // }}}

  void ICLWidget::contrastChanged(int val){
    // {{{ open

    m_data->bci[1] = val;
    if(*m_data->bciUpdateAuto){
      rebufferImageInternal();
    }
  }

  // }}}

  void ICLWidget::intensityChanged(int val){
    // {{{ open

    m_data->bci[2] = val;
    if(*m_data->bciUpdateAuto){
      rebufferImageInternal();
    }
  }

  // }}}
  
  void ICLWidget::scaleModeChanged(int modeIdx){
    // {{{ open

    //hold aspect ratio,force fit,no scale, zoom
    switch(modeIdx){
      case 0: m_data->fm = fmNoScale; break;
      case 1: m_data->fm = fmHoldAR; break;
      case 2: m_data->fm = fmFit; break;
      case 3: m_data->fm = fmZoom; break;
      default: ERROR_LOG("invalid scale mode index");
    }
    update();
  }

  // }}}

  void ICLWidget::currentChannelChanged(int modeIdx){
    // {{{ open

    m_data->selChannel = modeIdx - 1;
    if(*m_data->channelUpdateAuto){
      rebufferImageInternal();
    }
  }

  // }}}
  
  void ICLWidget::setViewPort(const Size &size){
    // {{{ open

    m_data->defaultViewPort = size;
    if(m_data->image.isNull()){
      m_data->imageInfoIndicator->update(m_data->defaultViewPort);
    }
      
  }

  // }}}

  void ICLWidget::showHideMenu(){
    // {{{ open

    if(!m_data->menuptr){
      create_menu(this,m_data);
    }

    m_data->menuptr->setVisible(!m_data->menuptr->isVisible());
    if(m_data->menuptr->isVisible()){
      if(m_data->selectedTabIndex == 4){
        m_data->infoTabVisible = true;
      }else{
        m_data->infoTabVisible = false;
      }
    }else{
      m_data->infoTabVisible = false;
    }
    m_data->adaptMenuSize(size());
  }

  // }}}
  
  void ICLWidget::setMenuEmbedded(bool embedded){
    // {{{ open

    if(!m_data->menuptr){
      create_menu(this,m_data);
    }

    bool visible = m_data->menuptr->isVisible();
    if(embedded){
      delete m_data->menuptr;
      create_menu(this,m_data);
      m_data->adaptMenuSize(size());
    }else{
      m_data->menuptr->setParent(0);
      m_data->menuptr->setWindowTitle("menu...");
      m_data->menuptr->setGeometry(QRect(mapToGlobal(pos())+QPoint(width()+5,0),QSize(450,350)));
    }
    m_data->menuptr->setVisible(visible);
  }

  // }}}

  void ICLWidget::recordButtonToggled(bool checked){
    // {{{ open

    if(!m_data->outputCap){
      m_data->outputCap = new OutputBufferCapturer(this,m_data);
    }
    if(checked){
      QMutexLocker l(&m_data->menuMutex);
      OutputBufferCapturer::CaptureTarget t = m_data->menu.getValue<ComboHandle>("auto-cap-mode").getSelectedIndex()?
                                              OutputBufferCapturer::FRAME_BUFFER :  OutputBufferCapturer::SET_IMAGES;

      const std::string params = m_data->menu.getValue<StringHandle>("auto-cap-filepattern").getCurrentText();
      const std::string device = m_data->menu.getValue<ComboHandle>("auto-cap-device").getSelectedItem();
      int frameSkip = m_data->menu.getValue<SpinnerHandle>("auto-cap-frameskip").getValue();
      
      bool forceParams = m_data->menu.getValue<bool>("auto-cap-force");
      Size dstSize = parse<Size>(m_data->menu.getValue<ComboHandle>("auto-cap-size").getSelectedItem());
      icl::format dstFmt = parse<icl::format>(m_data->menu.getValue<ComboHandle>("auto-cap-format").getSelectedItem());
      icl::depth dstDepth = parse<icl::depth>(m_data->menu.getValue<ComboHandle>("auto-cap-depth").getSelectedItem());

      bool ok = m_data->outputCap->startRecording(t,device,params,frameSkip,forceParams,dstSize,dstFmt,dstDepth);
      if(!ok){
        (*m_data->menu.getValue<ButtonHandle>("auto-cap-record"))->setChecked(false);
      }else{
        m_data->glbuttons.back()->visible = true;
        update();
      }
    }else{
      m_data->outputCap->stopRecording();
      m_data->glbuttons.back()->visible = false;
      update();
    }
  }

  // }}}

  void ICLWidget::pauseButtonToggled(bool checked){
    // {{{ open

    if(!m_data->outputCap){
      m_data->outputCap = new OutputBufferCapturer(this,m_data);
    }
    m_data->outputCap->setPaused(checked);
  }

  // }}}

  void ICLWidget::stopButtonClicked(){
    // {{{ open
    if(m_data->outputCap){
      m_data->outputCap->stopRecording();
      QMutexLocker l(&m_data->menuMutex);
      (*m_data->menu.getValue<ButtonHandle>("auto-cap-record"))->setChecked(false);
    }
    m_data->glbuttons[5]->visible = false;
    update();
  }

  // }}}
  
  void ICLWidget::skipFramesChanged(int frameSkip){
    // {{{ open

    if(m_data->outputCap){
      m_data->outputCap->frameSkip = frameSkip;
    }
  }

  // }}}

  void ICLWidget::menuTabChanged(int index){
      // {{{ open
    m_data->selectedTabIndex = index;
    if(index == 5){
      m_data->infoTabVisible = true;
      updateInfoTab();
    }else{
      m_data->infoTabVisible = false;
    }
  }

  // }}}

  void ICLWidget::histoPanelParamChanged(){
    // {{{ open
    QMutexLocker l(&m_data->menuMutex);
    if(!m_data->histoWidget) return;
    m_data->histoWidget->setFeatures(m_data->menu.getValue<bool>("log-on"),
                                     m_data->menu.getValue<bool>("blur-on"),
                                     m_data->menu.getValue<bool>("median-on"),
                                     m_data->menu.getValue<bool>("fill-on"),
                                     m_data->menu.getValue<ComboHandle>("histo-channel").getSelectedIndex()-1);

    m_data->histoWidget->update();
    
  }
  // }}}
 
  void ICLWidget::updateInfoTab(){
    // {{{ open

    if(m_data->menuMutex.tryLock()){
      if(m_data->histoWidget && m_data->infoTabVisible){
        m_data->histoWidget->update(getImageStatistics());
        // xxx TODO
        std::vector<string> s = getImageInfo();
        m_data->menu.getValue<LabelHandle>("image-info-label") = cat(s,"\n");
      }
      m_data->menuMutex.unlock();
    }

  }

  // }}}
  
  std::string ICLWidget::getImageCaptureFileName(){
    // {{{ open

    QMutexLocker l(&m_data->menuMutex);
    if(!m_data->menu.getDataStore().contains("cap-filename")){
      return "image.pnm";
    }
    ComboHandle &h = m_data->menu.getValue<ComboHandle>("cap-filename");
    std::string filename="image.pnm";
    switch(h.getSelectedIndex()){
      case 1:{
        string t = Time::now().toString();
        for(unsigned int i=0;i<t.length();i++){
          if(t[i]=='/')t[i]='.';
          if(t[i]==' ')t[i]='_';
        }
        filename = str("image_")+t+str(".pnm");
        break;
      }
      case 2:{
        m_data->menuMutex.unlock();
        QString name = QFileDialog::getSaveFileName(this,"filename ...","./",
                                                    "common formats (*.pnm *.ppm *.pgm *.jpg *.png)\nall files (*.*)");
        m_data->menuMutex.lock();
        filename = name.toLatin1().data();
        break;
      }
      default:
        break;
    }
    return filename;
  }

  // }}}

  void ICLWidget::captureCurrentImage(){
    // {{{ open

    const ImgBase *buf = 0;
    {
      LOCK_SECTION;
      if(!m_data->image.isNull()){
        buf = m_data->image.extractImage();
      }
    }

    if(buf){
      std::string filename = getImageCaptureFileName();
      if(filename != ""){
        try{
          FileWriter(filename).write(buf);          
        }catch(const ICLException &ex){
          ERROR_LOG("unable to capture current image: " << ex.what());
        }
      }
    }
  }

  // }}}

  const Img8u &ICLWidget::grabFrameBufferICL(){
    // {{{ open

    if(!m_data->qic){
      m_data->qic = new QImageConverter;
    }
    QImage qim = grabFrameBuffer();
    m_data->qic->setQImage(&qim);
    return *m_data->qic->getImg<icl8u>();
  }

  // }}}
  
  void ICLWidget::captureCurrentFrameBuffer(){
    // {{{ open

    const Img8u &fb = grabFrameBufferICL();
    std::string filename = getImageCaptureFileName();
    if(filename == ""){
      ERROR_LOG("current capture filename is empty");
      return;
    } 
    
    try{
      FileWriter(filename).write(&fb);
    }catch(ICLException &ex){
      ERROR_LOG("error capturing frame buffer: " << ex.what());
    }
  }

  // }}}
  
  void ICLWidget::rebufferImageInternal(){
    // {{{ open
    /*
        m_data->mutex.lock();
        if(m_data-m_data->image && m_data->image->hasImage()){
        if(m_data->channelSelBuf){
        m_data->mutex.unlock();
        setImage(m_data->channelSelBuf);
        }else{
        ImgBase *tmpImage = m_data->image->deepCopy();
        m_data->mutex.unlock();
        setImage(tmpImage);
        delete tmpImage;
        }
        }else{
        m_data->mutex.unlock();
        }
    */
    WARNING_LOG("rebuffering was disabled!");
    update();
  }

  // }}}
  
  void ICLWidget::initializeGL(){
    // {{{ open

    glClearColor (0.0, 0.0, 0.0, 0.0);
    //    glShadeModel(GL_FLAT);
    glEnable(GL_TEXTURE_2D);
    //glDisable(GL_DEPTH_TEST);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    glEnable(GL_BLEND);

    glOrtho(0, width(), height(), 0, -999999, 999999);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
  }

  // }}}

  void ICLWidget::resizeGL(int w, int h){
    // {{{ open

    LOCK_SECTION;
    makeCurrent();
    glViewport(0, 0, (GLint)w, (GLint)h);
  }

  // }}}
 
  void ICLWidget::paintGL(){
    // {{{ open
    //    m_data->mutex.lock();
    
    LOCK_SECTION;
    glClearColor(m_data->backgroundColor[0],m_data->backgroundColor[1],
                 m_data->backgroundColor[2],1);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT );
    GLPaintEngine *pe = 0;
    if(!m_data->image.isNull()){
      Rect r;
      if(m_data->fm == fmZoom){
        QMutexLocker locker(&m_data->menuMutex);
        r = computeRect(m_data->image.getSize(),getSize(),fmZoom,m_data->zoomRect);//zoomAdjuster->aw->r);
      }else{
        r = computeRect(m_data->image.getSize(),getSize(),m_data->fm);
      }
      m_data->image.setScaleMode(m_data->useLinInterpolation?interpolateLIN:interpolateNN);
      m_data->image.draw2D(r,getSize());
    }else{
      pe = new GLPaintEngine(this);
      pe->fill(0,0,0,255);
      pe->rect(Rect(0,0,width(),height()));
      
      if(m_data->showNoImageWarnings){
        pe->color(0,100,255,230);
        pe->fontsize(12);
        pe->text(Rect(0,0,width(),height()),"[null]");
      }
    }

    if(!pe) pe = new GLPaintEngine(this);
    pe->color(255,255,255,255);
    pe->fill(255,255,255,255);
    customPaintEvent(pe);
    
    if(m_data->outputCap){
      m_data->outputCap->captureFrameBufferHook();
    }

    if(m_data->embeddedZoomRect){
      pe->color(0,150,255,200);
      pe->fill(0,150,255,50);
      Rect32f &r = *m_data->embeddedZoomRect; 
      pe->rect(Rect((int)r.x,(int)r.y,(int)r.width,(int)r.height));
    }

    m_data->event(0,0,OSDGLButton::Draw);

    ICL_DELETE(pe);
  }

  // }}}

  void ICLWidget::paintEvent(QPaintEvent *e){
    // {{{ open

    QGLWidget::paintEvent(e);    
  }

  // }}}

  void ICLWidget::setImage(const ImgBase *image){ 
    // {{{ open

    LOCK_SECTION;
    if(!image){
      m_data->image.update(0);
      m_data->imageInfoIndicator->update(m_data->defaultViewPort);
      return;
    }

    update_data(image->getSize(),m_data);
    
    if(m_data->rm == rmAuto){
      m_data->image.setBCI(-1,-1,-1);
    }else if(m_data->rm == rmOn){
      m_data->image.setBCI(m_data->bci[0],m_data->bci[1],m_data->bci[2]);
    }else{
      m_data->image.setBCI(0,0,0);
    }

    if(m_data->selChannel >= 0 && m_data->selChannel < image->getChannels()){
      const ImgBase *selectedChannel = image->selectChannel(m_data->selChannel);
      m_data->image.update(selectedChannel);
      delete selectedChannel;
      if(image != m_data->channelSelBuf){
        image->deepCopy(&m_data->channelSelBuf);
      }
    }else{
      m_data->image.update(image);
      ICL_DELETE(m_data->channelSelBuf);
    }

    if(m_data->outputCap){
      m_data->outputCap->captureImageHook();
    }
    m_data->imageInfoIndicator->update(image->getParams(),image->getDepth());
    updateInfoTab();
  }

  // }}}

  void ICLWidget::setFitMode(fitmode fm){
    // {{{ open
    m_data->fm = fm;
  }

  // }}}

  void ICLWidget::setRangeMode(rangemode rm){
    // {{{ open
    m_data->rm = rm;
  }

  // }}}

  void ICLWidget::setBCI(int brightness, int contrast, int intensity){
    // {{{ open
    m_data->bci[0] = brightness;
    m_data->bci[1] = contrast;
    m_data->bci[2] = intensity;
  }
  // }}}

  void ICLWidget::customPaintEvent(PaintEngine*){
    // {{{ open
  }
  // }}}

  void ICLWidget::mousePressEvent(QMouseEvent *e){
    // {{{ open

    if(m_data->embeddedZoomMode){
      m_data->embeddedZoomRect = new Rect32f(e->x(),e->y(),0.1,0.1);
      update();
      return;
    }

    if(m_data->event(e->x(),e->y(),OSDGLButton::Press)){
      update();
      return;
    }

    switch(e->button()){
      case Qt::LeftButton: m_data->downMask[0]=true; break;
      case Qt::RightButton: m_data->downMask[2]=true; break;
      default: m_data->downMask[1] = true; break;
    }
    emit mouseEvent(createMouseEvent(MousePressEvent));
    update();
  }

  // }}}

  void ICLWidget::mouseReleaseEvent(QMouseEvent *e){
    // {{{ open

    if(m_data->embeddedZoomMode){
      if(m_data->embeddedZoomModeJustEnabled){
        m_data->embeddedZoomModeJustEnabled = false;
      }else{
        if(m_data->embeddedZoomRect){
          Rect32f &r = *m_data->embeddedZoomRect;
          //const Size wSize(width(),height());
          Rect ir = getImageRect();
          
          Rect32f &nr = m_data->zoomRect;
          nr.x = (r.x-ir.x)/float(ir.width);
          nr.y = (r.y-ir.y)/float(ir.height);
          nr.width = r.width/ir.width;
          nr.height = r.height/ir.height;
          nr = fixRectAR(nr,this);
          // xxxxx
          /*
              m_data->zoomRect.x = r.x/wSize.width;
              m_data->zoomRect.y = r.y/wSize.height;
              m_data->zoomRect.width = r.width/wSize.width;
              m_data->zoomRect.height = r.height/wSize.height;
          */
          m_data->pushScaleModeAndChangeToZoom();
        }
        ICL_DELETE(m_data->embeddedZoomRect);
        m_data->embeddedZoomMode = false;
        m_data->embeddedZoomModeJustEnabled = false;
        update();
        return;
      }
    }

    if(m_data->event(e->x(),e->y(),OSDGLButton::Release)){
      update();
      return;
    }

    switch(e->button()){
      case Qt::LeftButton: m_data->downMask[0]=false; m_data->lastMouseReleaseButton = 0; break;
      case Qt::RightButton: m_data->downMask[2]=false; m_data->lastMouseReleaseButton = 2; break;
      default: m_data->downMask[1] = false; m_data->lastMouseReleaseButton = 1; break;
    }
    emit mouseEvent(createMouseEvent(MouseReleaseEvent));
    update();
  }

  // }}}

  void ICLWidget::mouseMoveEvent(QMouseEvent *e){
    // {{{ open

    if(m_data->embeddedZoomMode && m_data->embeddedZoomRect){
      m_data->embeddedZoomRect->width = e->x()-m_data->embeddedZoomRect->x;
      m_data->embeddedZoomRect->height = e->y()-m_data->embeddedZoomRect->y;
      *m_data->embeddedZoomRect = m_data->embeddedZoomRect->normalized();
      update();
      return;
    }


    if(m_data->event(e->x(),e->y(),OSDGLButton::Move)){
      update();
      return;
    }
    m_data->mouseX = e->x();
    m_data->mouseY = e->y();
    
    if(m_data->downMask[0] || m_data->downMask[1] || m_data->downMask[2]){
      emit mouseEvent(createMouseEvent(MouseDragEvent));
    }else{
      emit mouseEvent(createMouseEvent(MouseMoveEvent));
    }

    if(m_data->imageInfoIndicatorEnabled){
      m_data->imageInfoIndicator->updateMousePos(m_data->mouseEvent.getPos());
    }

    update();
  }

  // }}}
  
  void ICLWidget::enterEvent(QEvent*){
    // {{{ open

    if(m_data->menuEnabled){
      if(m_data->event(-1,-1,OSDGLButton::Enter)){
        update();
        return;
      }
    }
    if(m_data->imageInfoIndicatorEnabled){
      m_data->imageInfoIndicator->show();
      m_data->updateImageInfoIndicatorGeometry(size());
    }
    emit mouseEvent(createMouseEvent(MouseEnterEvent));
    update();
  }

  // }}}

  void ICLWidget::leaveEvent(QEvent*){
    // {{{ open

    if(m_data->menuEnabled){
      if(m_data->event(-1,-1,OSDGLButton::Leave)){
        update();
        return;
      }
    }
    if(m_data->imageInfoIndicatorEnabled){
      m_data->imageInfoIndicator->hide();
    }

    m_data->downMask[0] = m_data->downMask[1] = m_data->downMask[2] = false;
    emit mouseEvent(createMouseEvent(MouseLeaveEvent));
    update();
  }

  // }}}

  void ICLWidget::wheelEvent(QWheelEvent *e){
    // {{{ open 

    // possibly adding a wheel base zooming if a certain keyboard modifier is pressed
    
    if(e->orientation() == Qt::Horizontal){
      m_data->wheelDelta = Point(e->delta(),0);
    }else{
      m_data->wheelDelta = Point(0,e->delta());
    }
    
    emit mouseEvent(createMouseEvent(MouseWheelEvent));

    update();
    //QGLWidget::wheelEvent(e);
#if 0
    if(m_data->embeddedZoomMode && m_data->embeddedZoomModeJustEnabled){
      return;
    }



    static const float zoominfac = 0.9;
    static const float zoomoutfac = 1.5;
    float steps = e->delta()/120.0; // delta is wheelangle in 1/8 degree !
    float zoom = (steps>0) ? pow(zoominfac,steps) : pow(zoomoutfac,-steps);
    Rect ir = getImageRect(true);
    Size is = getImageSize(true);
    Size ws = getSize(); // widget size
    Rect32f &zr = m_data->zoomRect;
    float relWidth=1, relHeight=1;
    float ar = float(is.width) / float(is.height);
    if (ar > 1){
      relWidth = ar; 
    }else{
      relHeight = 1.0/ar;
    }
    //    if(zr == Rect32f(0,0,0,0)){
    if(getFitMode() != fmZoom){
      zr = Rect32f(-float(ir.x)/ws.width,-float(ir.y)/ws.height,relHeight,relWidth);
      //zr = Rect32f(-float(ir.x)/ws.width,0,relHeight,relWidth);
      zr = fixRectAR(zr,this);
    }
    // Compute current relative image position
    float boxX = e->x() - ir.x;
    float boxY = e->y() - ir.y;
    int imageX = (int) rint((boxX*(is.width))/ir.width);
    int imageY = (int) rint((boxY*(is.height))/ir.height);
    float relImageX = float(imageX)/is.width;
    float relImageY = float(imageY)/is.height;
    
    Rect32f newZR = zr;
    newZR.width *= zoom;
    newZR.height *= zoom;
    
    newZR.x = relImageX - zoom *(relImageX-zr.x);
    newZR.y = relImageY - zoom *(relImageY-zr.y);
//    SHOW(newZR);
    zr = newZR;
    if(zr.width >=1 && zr.height >=1){
      DEBUG_LOG("resetting fitmode");
      setFitMode(fmHoldAR);
      zr = Rect32f(0,0,0,0);
    }else{
      setFitMode(fmZoom);    
    }

    update();
    if(m_data->zoomAdjuster) m_data->zoomAdjuster->update();
#endif
  }

  // }}}

  void ICLWidget::resizeEvent(QResizeEvent *e){
    // {{{ open
    resizeGL(e->size().width(),e->size().height());
    m_data->adaptMenuSize(size());
    if(m_data->embeddedZoomMode || m_data->fm == fmZoom){
      m_data->zoomRect = fixRectAR(m_data->zoomRect,this);
      if(m_data->fm == fmZoom){
        if(m_data->zoomAdjuster) m_data->zoomAdjuster->update();
      }
    }
  }
  // }}}

  void ICLWidget::setVisible(bool visible){
    // {{{ open
    QGLWidget::setVisible(visible);
    // xxx m_data->showMenuButton->setVisible(false);
    // xxx m_data->embedMenuButton->setVisible(false);
    m_data->imageInfoIndicator->setVisible(false);
    if(m_data->menuptr){
      m_data->menuptr->setVisible(false);
      m_data->adaptMenuSize(size());
    }
  }

  // }}}

  ICLWidget::fitmode ICLWidget::getFitMode(){
    // {{{ open

    return m_data->fm;
  }

  // }}}
  
  ICLWidget::rangemode ICLWidget::getRangeMode(){
    // {{{ open

    return m_data->rm;
  }

  // }}}

  void ICLWidget::setShowNoImageWarnings(bool showWarnings){
    // {{{ open

    m_data->showNoImageWarnings = showWarnings;
  }

  // }}}

  void ICLWidget::updateFromOtherThread(){
    // {{{ open

    QApplication::postEvent(this,new QEvent(QEvent::User),Qt::HighEventPriority);
  }

  // }}}

  void ICLWidget::setMenuEnabled(bool enabled){
    // {{{ open

    m_data->menuEnabled = enabled;
  }

  // }}}

  void ICLWidget::setImageInfoIndicatorEnabled(bool enabled){
    // {{{ open
    m_data->imageInfoIndicatorEnabled = enabled;
  }
  // }}}
 
  void ICLWidget::setShowPixelGridEnabled(bool enabled){
    // {{{ open
    m_data->image.setDrawGrid(enabled,m_data->gridColor);
    updateFromOtherThread();
  }
  // }}}

  void ICLWidget::setRangeModeNormalOrScaled(bool enabled){
    // {{{ open
    setRangeMode(enabled?rmAuto:rmOff);
    rebufferImageInternal();
    if(!m_data->menuptr){
      create_menu(this,m_data);
      showHideMenu();
      showHideMenu();
    }
    ComboHandle ch = m_data->menu.getValue<ComboHandle>("bci-mode");
    ch.setSelectedIndex(enabled?2:1);
  }
  // }}}

  void ICLWidget::showBackgroundColorDialog(){
    // {{{ open

    float *o = m_data->backgroundColor;
    QColor color = QColorDialog::getColor(QColor(o[0]*255,o[1]*255,o[2]*255),this);
    o[0] = float(color.red())/255;
    o[1] = float(color.green())/255;
    o[2] = float(color.blue())/255;
    updateFromOtherThread();
  }

  // }}}
  void ICLWidget::setBackgroundBlack(){
    // {{{ open
    std::fill(m_data->backgroundColor,m_data->backgroundColor+3,0);
    updateFromOtherThread();
  }
  // }}}
  
  void ICLWidget::setBackgroundWhite(){
    // {{{ open
    std::fill(m_data->backgroundColor,m_data->backgroundColor+3,1);
    updateFromOtherThread();
  }
  // }}}
  void ICLWidget::setBackgroundGray(){
    // {{{ open
    std::fill(m_data->backgroundColor,m_data->backgroundColor+3,0.3);
    updateFromOtherThread();
  }
  // }}}

  void ICLWidget::showGridColorDialog(){
    // {{{ open
    const float *g = m_data->image.getGridColor();
    QColor color = QColorDialog::getColor(QColor(g[0]*255,g[1]*255,g[2]*255),this);
    float n[4] = { float(color.red())/255, float(color.green())/255, float(color.blue())/255, g[3]};
    m_data->image.setGridColor(n);
    updateFromOtherThread();
  }
  // }}}
  
  void ICLWidget::setGridBlack(){
    // {{{ open
    float c[4] = {0,0,0,1};
    m_data->image.setGridColor(c);
    updateFromOtherThread();
  }
  // }}}
  void ICLWidget::setGridWhite(){
    // {{{ open
    float c[4] = {1,1,1,1};
    m_data->image.setGridColor(c);
    updateFromOtherThread();
  }
  // }}}
  void ICLWidget::setGridGray(){
    // {{{ open
    float c[4] = {0.3,0.3,0.3,1};
    m_data->image.setGridColor(c);
    updateFromOtherThread();
  }
  // }}}

  void ICLWidget::setGridAlpha(int alpha){
    // {{{ open 
    const float *c = m_data->image.getGridColor();
    float n[4] = {c[0],c[1],c[2],float(alpha)/255};
    m_data->image.setGridColor(n);
    updateFromOtherThread();
  }
  // }}}
  
  std::vector<std::string> ICLWidget::getImageInfo(){
    // {{{ open
    std::vector<string> info;

    GLImg &i = m_data->image;
    if(i.isNull()){
      info.push_back("Image is NULL");
      return info;
    }
    info.push_back(string("depth:   ")+str(i.getDepth()));
    info.push_back(string("size:    ")+str(i.getSize()));
    info.push_back(string("channels:")+str(i.getChannels()));
    info.push_back(string("format:  ")+str(i.getFormat()));
    if(i.getROI() == Rect(Point::null,i.getSize())){
      info.push_back("roi:   full");
    }else{
      info.push_back(str(i.getROI()));
    }
    
    std::vector<Range<icl64f> > ranges = i.getMinMax();
    for(int a=0;a<i.getChannels();a++){
      info.push_back(str("channel "+str(a)+":"));
      info.push_back(str("   ")+str(ranges[a]));
    }

    info.push_back(string("time:  ")+str(i.getTime()));
    return info;
  }

  // }}}

  Size ICLWidget::getImageSize(bool fromGUIThread){
    // {{{ open
    if(fromGUIThread){
      m_data->mutex.lock();
    }
    Size s;
    if(!m_data->image.isNull()){
      s = m_data->image.getSize(); 
    }else{
      s = m_data->defaultViewPort;
    }
    if(fromGUIThread){
      m_data->mutex.unlock();
    }

    return s;
  }

  // }}}

  Rect ICLWidget::getImageRect(bool fromGUIThread){
    // {{{ open
    if(fromGUIThread){
      m_data->mutex.lock();
    }

    Rect r;
    if(m_data->fm == fmZoom){
      QMutexLocker locker(&m_data->menuMutex);
      r = computeRect(!m_data->image.isNull() ? m_data->image.getSize() : m_data->defaultViewPort, getSize(),fmZoom,m_data->zoomRect);
    }else{
      r = computeRect(!m_data->image.isNull() ? m_data->image.getSize() : m_data->defaultViewPort, getSize(),m_data->fm);
    }
    if(fromGUIThread){
      m_data->mutex.unlock();
    }

    return r;
  }

  // }}}

  const MouseEvent &ICLWidget::createMouseEvent(MouseEventType type){
    // {{{ open


    MouseEvent &evt = m_data->mouseEvent;

    LOCK_SECTION;

    if(type == MouseReleaseEvent){
      m_data->downMask[m_data->lastMouseReleaseButton] = true;
    }

#if 0
    if(!m_data->image || !m_data->image->hasImage()){
      const Point &wheelDelta = (type == MouseWheelEvent) ? m_data->wheelDelta : Point::null;
      evt = MouseEvent(Point(m_data->mouseX,m_data->mouseY),
                       Point(-1,-1),
                       Point32f(-1,-1),
                       Point32f((float)m_data->mouseX/float(width()),(float)m_data->mouseY/float(height())),
                       m_data->downMask,
                       std::vector<double>(),
		       wheelDelta,
                       type,this);
      if(type == MouseReleaseEvent){
        m_data->downMask[m_data->lastMouseReleaseButton] = false;
      }
      return evt;
    
    }else{
#endif
      
      Rect r = getImageRect();
      int iw = !m_data->image.isNull() ? m_data->image.getWidth() : m_data->defaultViewPort.width;
      int ih = !m_data->image.isNull() ? m_data->image.getHeight() : m_data->defaultViewPort.height;
      float boxX = m_data->mouseX - r.x;
      float boxY = m_data->mouseY - r.y;
      float imageX32f = (boxX*iw)/float(r.width);
      float imageY32f = (boxY*ih)/float(r.height);
      int imageX = (int) rint(-0.5+(boxX*iw)/r.width);
      int imageY = (int) rint(-0.5+(boxY*ih)/r.height);

      float relImageX = float(imageX)/iw;
      float relImageY = float(imageY)/ih;

      std::vector<double> color;
      if(!m_data->image.isNull() && r.contains(m_data->mouseX,m_data->mouseY)){
        color = m_data->image.getColor(imageX,imageY);
      }
      
      const Point &wheelDelta = (type == MouseWheelEvent) ? m_data->wheelDelta : Point::null;
      evt = MouseEvent(Point(m_data->mouseX,m_data->mouseY),
                       Point(imageX,imageY),
                       Point32f(imageX32f,imageY32f),
                       Point32f(relImageX,relImageY),
                       m_data->downMask,
                       color,
                       wheelDelta,
                       type,this);
      if(type == MouseReleaseEvent){
        m_data->downMask[m_data->lastMouseReleaseButton] = false;
      }
      return evt;
#if 0
    }
#endif
  }

  // }}}

  const ImageStatistics &ICLWidget::getImageStatistics() {
    // {{{ open

    if(!m_data->image.isNull()){
      return m_data->image.getStats();
    }else{
      static ImageStatistics s;
      s.isNull = true;
      return s;
    }
  }
  // }}}

  void ICLWidget::install(MouseHandler *h){
    // {{{ open

    connect(this,SIGNAL(mouseEvent(const MouseEvent&)),
            h,SLOT(handleEvent(const MouseEvent&)));  
  }

  // }}}

  void ICLWidget::uninstall(MouseHandler *h){
    // {{{ open

    disconnect(this,SIGNAL(mouseEvent(const MouseEvent &)),
               h,SLOT(handleEvent(const MouseEvent &)));  
  }

  // }}}

  bool ICLWidget::event(QEvent *event){
    // {{{ open

    ICLASSERT_RETURN_VAL(event,false);
    if(event->type() == QEvent::User){
      update();
      return true;
    }else{
      return QGLWidget::event(event);
    }
  } 

  // }}}

  void ICLWidget::registerCallback(const GUI::Callback &cb, const std::string &eventList){
    // {{{ open

    struct CallbackHandler : public MouseHandler{
      GUI::Callback cb;
      std::vector<MouseEventType> evts;
      bool m_all;
      CallbackHandler(const GUI::Callback &cb ,const std::string &eventList):
        cb(cb),m_all(false){
        std::vector<std::string> eventVec = icl::tok(eventList,",");
        for(unsigned int i=0;i<eventVec.size();++i){
          const std::string &e = eventVec[i];
          if(e == "all"){
            m_all = true;
            break;
          }else if(e == "move"){
            evts.push_back(MouseMoveEvent);
          }else if(e == "drag"){
            evts.push_back(MouseDragEvent);
          }else if(e == "press"){
            evts.push_back(MousePressEvent);
          }else if(e == "release"){
            evts.push_back(MouseReleaseEvent);
          }else if(e == "enter"){
            evts.push_back(MouseEnterEvent);
          }else if(e == "leave"){
            evts.push_back(MouseLeaveEvent);
          }
        }
      }
      virtual void process(const MouseEvent &evt){
        if(m_all){
          cb();
          return;
        }
        MouseEventType t = evt.getType();
        for(unsigned int i=0;i<evts.size();++i){
          if(evts[i] == t){
            cb();
            return;
          }
        }
      }
    };
    MouseHandler *cbh = new CallbackHandler(cb,eventList);
    m_data->callbacks.push_back(cbh);
    install(cbh);
  }

  // }}}
  
  void ICLWidget::removeCallbacks(){
    // {{{ open

    m_data->deleteAllCallbacks();
  }

  // }}}
}

