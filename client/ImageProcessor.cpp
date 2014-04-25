
#ifdef IMAGETEST 
#include <stdint.h>
#else
#include <ImageProcessor.hpp>
#endif


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

static int crop_border(CImg<float>& img)
{
    static const float SD_DIFF_THRESHOLD = 10.0;
    static const float AVG_DIFF_THRESHOLD = 255.0;
    int x1, x2, y1, y2;
    float last_avg = 0;

    x1 = x2 = y1 = y2 = 0;
    for(int y = 0; y < img.height() / 2; y++) {
        float sum = 0, avg = 0; 
        int count = img.width();
        for(int x = 0; x < img.width(); x++) {
            sum += *(img.data(x, y));
        }
        avg = sum / count;
        sum = 0;
        for(int x = 0; x < img.width(); x++) {
            sum += pow(avg - *(img.data(x, y)), 2);
        }
        sum /= count;
        sum = sqrt(sum);
        float avg_diff = avg - last_avg;
        if (avg_diff < 0) avg_diff = -avg_diff;
        if (y == 0)
            last_avg = avg;
        y1 = y;
        if (sum > SD_DIFF_THRESHOLD || 
                (y != 0 && avg_diff > AVG_DIFF_THRESHOLD)) {
            break;
        }
    }

    for(int y = 0; y < img.height() / 2; y++) {
        float sum = 0, avg = 0; 
        int count = img.width();
        for(int x = 0; x < img.width(); x++) {
            sum += *(img.data(x, img.height() - 1 - y));
        }
        avg = sum / count;
        sum = 0;
        for(int x = 0; x < img.width(); x++) {
            sum += pow(avg - *(img.data(x, img.height() - 1 - y)), 2);
        }
        sum /= count;
        sum = sqrt(sum);
        float avg_diff = avg - last_avg;
        if (avg_diff < 0) avg_diff = -avg_diff;
        if (y == 0)
            last_avg = avg;
        y2 = img.height() - y;
        if (sum > SD_DIFF_THRESHOLD || 
                (y != 0 && avg_diff > AVG_DIFF_THRESHOLD)) {
            break;
        }
    }

    for(int x = 0; x < img.width() / 2; x++) {
        float sum = 0, avg = 0; 
        int count = img.width();
        for(int y = 0; y < img.height(); y++) {
            sum += *(img.data(x, y));
        }
        avg = sum / count;
        sum = 0;
        for(int y = 0; y < img.height(); y++) {
            sum += pow(avg - *(img.data(x, y)), 2);
        }
        sum /= count;
        sum = sqrt(sum);
        float avg_diff = avg - last_avg;
        if (avg_diff < 0) avg_diff = -avg_diff;
        if (x == 0)
            last_avg = avg;
        x1 = x;
        if (sum > SD_DIFF_THRESHOLD || 
                (x != 0 && avg_diff > AVG_DIFF_THRESHOLD)) {
            break;
        }
    }

    for(int x = 0; x < img.width() / 2; x++) {
        float sum = 0, avg = 0; 
        int count = img.width();
        for(int y = 0; y < img.height(); y++) {
            sum += *(img.data(img.width() - 1 - x, y));
        }
        avg = sum / count;
        sum = 0;
        for(int y = 0; y < img.height(); y++) {
            sum += pow(avg - *(img.data(img.width() - 1 - x, y)), 2);
        }
        sum /= count;
        sum = sqrt(sum);
        float avg_diff = avg - last_avg;
        if (avg_diff < 0) avg_diff = -avg_diff;
        if (x == 0)
            last_avg = avg;
        x2 = img.width() - x;
        if (sum > SD_DIFF_THRESHOLD || 
                (x != 0 && avg_diff > AVG_DIFF_THRESHOLD)) {
            break;
        }
    }

    int nw = x2 - x1;
    int nh = y2 - y1;
    /* too small after crop, useless */
    if (nh < 32 || nw < 32)
        return -1;
    x1 += nw * 0.1;
    x2 -= nw * 0.1;
    y1 += nh * 0.1;
    y2 -= nh * 0.1;

    img.crop(x1, y1, 0, x2, y2, 0);
    return 0;
}

static void mirror(CImg<float>& img)
{
    float suma, sumb;
    /*
    suma = sumb = 0;
    for(int y = 0; y < img.height() / 2; y++) {
        for(int x = 0; x < img.width(); x++) {
            suma += *(img.data(x, y));
            sumb += *(img.data(x, img.height() - 1 - y));
        }
    }
    if (suma < sumb) 
        img.mirror('y');
    */

    suma = sumb = 0;
    for(int x = 0; x < img.width() / 2; x++) {
        for(int y = 0; y < img.height(); y++) {
            suma += *(img.data(x, y));
            sumb += *(img.data(img.width() - 1 - x, y));
        }
    }
    if (suma < sumb) 
        img.mirror('x');
}

int GetHashCode(const char *filename, uint64_t& result)
{
    result = 0x0000000000000000;
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
    CImg<float> c0;

    if (src.spectrum() == 3){
        c0 = src.RGBtoYCbCr().channel(0);
    } else if (src.spectrum() == 4) {
	    int width = img.width();
        int height = img.height();
        int depth = img.depth();
	    c0 = src.crop(0,0,0,0,width-1,height-1,depth-1,2)
            .RGBtoYCbCr().channel(0);
    } else {
	    c0 = src.channel(0);
    }

    if (crop_border(c0) < 0) {
        return 0;
    }
    mirror(c0);
    img = c0.get_convolve(meanfilter);
    img.resize(32,32);
    CImg<float> *C  = ph_dct_matrix(32);
    CImg<float> Ctransp = C->get_transpose();
    CImg<float> dctImage = (*C)*img*Ctransp;
    CImg<float> subsec = dctImage.crop(1,1,8,8).unroll('x');;
   
    float median = subsec.median();
    uint64_t one = 0x0000000000000001;
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

#ifdef IMAGETEST 
#include <dirent.h>

using namespace VideoMatch;

int main(int argc, char *argv[])
{
    struct dirent **filelist;
    int fnum = scandir(argv[1], &filelist, 0, alphasort);
    for(int i = 0; i < fnum; i++) {
        char filename[128];
        uint64_t hresult;
        int ret;

        const char *sfx = filelist[i]->d_name + strlen(filelist[i]->d_name) - 4;
        if (strcmp(sfx, ".jpg"))
            continue;
        snprintf(filename, 128, "%s/%s", argv[1], filelist[i]->d_name);
        ret = GetHashCode(filename, hresult);
        printf("%d %llx\n", atoi(filelist[i]->d_name), hresult);
        free(filelist[i]);
    }
    //free(filelist);
 
    return 0;
}

#endif
