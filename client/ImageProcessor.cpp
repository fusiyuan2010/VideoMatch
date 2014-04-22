
#include <ImageProcessor.hpp>


#define cimg_use_jpeg
#include <CImg.h>

using namespace cimg_library;


namespace VideoMatch {

static CImg<float>* ph_dct_matrix(const int N) 
{
    CImg<float> *ptr_matrix = new CImg<float>(N,N,1,1,1/sqrt((float)N));
    const float c1 = sqrt(2.0/N);
    for (int x=0;x<N;x++){
    for (int y=1;y<N;y++){
        *ptr_matrix->data(x,y) = c1*cos((cimg::PI/2/N)*y*(2*x+1));
    }
    }
    return ptr_matrix;
}

int GetHashCode(const char *filename, uint64_t& result)
{
    if (!filename) {
	    return -1;
    }

    CImg<uint8_t> src;
    try {
	    src.load(filename);
    } catch (CImgIOException ex){
	    return -1;
    }

    CImg<float> meanfilter(7,7,1,1,1);
    CImg<float> img;

    if (src.spectrum() == 3){
        img = src.RGBtoYCbCr().channel(0).get_convolve(meanfilter);
    } else if (src.spectrum() == 4) {
	    int width = img.width();
        int height = img.height();
        int depth = img.depth();
	    img = src.crop(0,0,0,0,width-1,height-1,depth-1,2)
            .RGBtoYCbCr().channel(0).get_convolve(meanfilter);
    } else {
	    img = src.channel(0).get_convolve(meanfilter);
    }

    img.resize(32,32);
    CImg<float> *C  = ph_dct_matrix(32);
    CImg<float> Ctransp = C->get_transpose();
    CImg<float> dctImage = (*C)*img*Ctransp;
    CImg<float> subsec = dctImage.crop(1,1,8,8).unroll('x');;
   
    float median = subsec.median();
    uint64_t one = 0x0000000000000001;
    result = 0x0000000000000000;
    for (int i=0;i< 64;i++){
	    float current = subsec(i);
        if (current > median)
	        result |= one;
	    one = one << 1;
    }
  
    delete C;

    return 0;
}


}

