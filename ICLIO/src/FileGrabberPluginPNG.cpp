/********************************************************************
**                Image Component Library (ICL)                    **
**                                                                 **
** Copyright (C) 2006-2010 CITEC, University of Bielefeld          **
**                         Neuroinformatics Group                  **
** Website: www.iclcv.org and                                      **
**          http://opensource.cit-ec.de/projects/icl               **
**                                                                 **
** File   : ICLIO/src/FileGrabberPluginPNG.cpp                     **
** Module : ICLIO                                                  **
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

#include <ICLIO/FileGrabberPluginPNG.h>
#include <ICLUtils/StringUtils.h>
#include <ICLCC/CCFunctions.h>
#include <png.h>

#include <stdio.h>

namespace icl{

  void FileGrabberPluginPNG::grab(File &file, ImgBase **dest){
    png_byte header[8];
    
    FILE *cfile = fopen(file.getName().c_str(), "rb");
    if(!cfile){
      ERROR_LOG("unable to open given file in read binary mode: " << file.getName());
      return;
    }
    fread(header, 1, 8, cfile);
    if (png_sig_cmp(header, 0, 8)){
      ERROR_LOG("given file's header " << file.getName() << " is not a valid png header");
      return;
    }      

    png_structp reader = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
    if(!reader){
      ERROR_LOG("unable to create png-read struct for file " << file.getName());
      return;
    }
    png_infop info = png_create_info_struct(reader); 
    if(!reader){
      ERROR_LOG("unable to create png-info struct for file " << file.getName());
      return;
    }
    
    if (setjmp(png_jmpbuf(reader))){
      ERROR_LOG("unable to setjmp(png_jmpbuf(reader)) for file " << file.getName());
      return;
    }
     
    png_init_io(reader,cfile);
    png_set_sig_bytes(reader,8);
    png_read_info(reader,info);

    int width = png_get_image_width(reader,info);
    int height = png_get_image_height(reader,info);
    int channels = png_get_channels(reader,info);
    png_byte colorfmt = png_get_color_type(reader,info);
    (void)colorfmt; // this is expanded to either rgb or gray
    png_byte bits = png_get_bit_depth(reader,info);

    // palette is automatically expanded to RGB / RGBA
    // gray is automatically expanded to at least 8Bit (+alpha if neccessary)
    png_set_expand(reader);
    
    int interlacing = png_set_interlace_handling(reader);
    (void)interlacing;
    png_read_update_info(reader,info);

    
    if (setjmp(png_jmpbuf(reader))){
      ERROR_LOG("unable to read png pixels for file " << file.getName());
      return;
    }
    int rowlen = png_get_rowbytes(reader,info);
    data.resize(height*rowlen);
    rows.resize(height);
    for(int i=0;i<height;++i){
      rows[i] = data.data() + i * rowlen;
    }
    
    png_read_image(reader,(png_bytep*)rows.data());
    fclose(cfile);

    if (setjmp(png_jmpbuf(reader))){
      ERROR_LOG("unable to release png reader structure " << file.getName());
      return;
    }
    png_destroy_read_struct(&reader,&info,NULL);

    depth destDepth = bits==16?depth16s:depth8u;
    
    switch(channels){
      case 1: // gray
        ensureCompatible(dest,destDepth, Size(width,height), formatGray);
        switch(bits){
          case 1: case 2: case 4: case 8: // these are now automatically expanded
            std::copy(data.begin(),data.end(),(*dest)->asImg<icl8u>()->begin(0)); break;
          case 16:             
            std::copy((const icl16s*)&data[0],(const icl16s*)&data[data.size()-1],
                      (*dest)->asImg<icl16s>()->begin(0));
            (*dest)->print();
            break;

          default: throw ICLException("error reading png image unexpected bit depth for 1 channel image");
        }
        break;
      case 2: // gray + alpha
        ensureCompatible(dest,destDepth, Size(width,height), 2);
        switch(bits){
          case 8: interleavedToPlanar(data.data(), (*dest)->asImg<icl8u>()); break; 
          case 16: interleavedToPlanar(data.data(), (*dest)->asImg<icl16s>()); break; 
          default: throw ICLException("error reading png image unexpected bit depth for 2 channel image");
        }
        break;
      case 3:
        ensureCompatible(dest,destDepth, Size(width,height), formatRGB);
        switch(bits){
          case 8: interleavedToPlanar(data.data(), (*dest)->asImg<icl8u>()); break; 
          case 16: interleavedToPlanar(data.data(), (*dest)->asImg<icl16s>()); break; 
          default: throw ICLException("error reading png image unexpected bit depth for 3 channel image");
        }
        break;
      case 4:
        ensureCompatible(dest,destDepth, Size(width,height), 4);
        switch(bits){
          case 8: interleavedToPlanar(data.data(), (*dest)->asImg<icl8u>()); break; 
          case 16: interleavedToPlanar(data.data(), (*dest)->asImg<icl16s>()); break; 
          default: throw ICLException("error reading png image unexpected bit depth for 4 channel image");
        }
      default: throw ICLException("error reading png image unexpected channel count");
    }
  }
}