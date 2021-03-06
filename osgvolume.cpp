/* OpenSceneGraph example, osgvolume.
*
*  Permission is hereby granted, free of charge, to any person obtaining a copy
*  of this software and associated documentation files (the "Software"), to deal
*  in the Software without restriction, including without limitation the rights
*  to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
*  copies of the Software, and to permit persons to whom the Software is
*  furnished to do so, subject to the following conditions:
*
*  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
*  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
*  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
*  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
*  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
*  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
*  THE SOFTWARE.
*/
#include "osgvolume.h"

#include <osg/Node>
#include <osg/Geometry>
#include <osg/Notify>
#include <osg/Texture3D>
#include <osg/Texture1D>
#include <osg/ImageSequence>
#include <osg/TexGen>
#include <osg/Geode>
#include <osg/Billboard>
#include <osg/PositionAttitudeTransform>
#include <osg/ClipNode>
#include <osg/ClipPlane>
#include <osg/AlphaFunc>
#include <osg/TexGenNode>
#include <osg/TexEnv>
#include <osg/TexEnvCombine>
#include <osg/Material>
#include <osg/PrimitiveSet>
#include <osg/Endian>
#include <osg/BlendFunc>
#include <osg/BlendEquation>
#include <osg/TransferFunction>
#include <osg/MatrixTransform>

#include <osgDB/Registry>
#include <osgDB/ReadFile>
#include <osgDB/WriteFile>
#include <osgDB/FileUtils>
#include <osgDB/FileNameUtils>

#include <osgGA/EventVisitor>
#include <osgGA/TrackballManipulator>
#include <osgGA/FlightManipulator>
#include <osgGA/KeySwitchMatrixManipulator>

#include <osgUtil/CullVisitor>

#include <osgViewer/Viewer>
#include <osgViewer/ViewerEventHandlers>

#include <osgManipulator/TabBoxDragger>
#include <osgManipulator/TabPlaneTrackballDragger>
#include <osgManipulator/TrackballDragger>

#include <osg/io_utils>

#include <algorithm>
#include <iostream>

#include <osg/ImageUtils>
#include <osgVolume/Volume>
#include <osgVolume/VolumeTile>
#include <osgVolume/RayTracedTechnique>
#include <osgVolume/FixedFunctionTechnique>

#define OMEGA_NO_GL_HEADERS
#include <omega.h>
#include <omegaToolkit.h>
#include <omegaOsg/omegaOsg.h>




osg::Image* createTexture3D(osg::ImageList& imageList,
            unsigned int numComponentsDesired,
            int s_maximumTextureSize,
            int t_maximumTextureSize,
            int r_maximumTextureSize,
            bool resizeToPowerOfTwo)
{

    if (numComponentsDesired==0)
    {
        return osg::createImage3DWithAlpha(imageList,
                                        s_maximumTextureSize,
                                        t_maximumTextureSize,
                                        r_maximumTextureSize,
                                        resizeToPowerOfTwo);
    }
    else
    {
        GLenum desiredPixelFormat = 0;
        switch(numComponentsDesired)
        {
            case(1) : desiredPixelFormat = GL_LUMINANCE; break;
            case(2) : desiredPixelFormat = GL_LUMINANCE_ALPHA; break;
            case(3) : desiredPixelFormat = GL_RGB; break;
            case(4) : desiredPixelFormat = GL_RGBA; break;
        }

        return osg::createImage3D(imageList,
                                        desiredPixelFormat,
                                        s_maximumTextureSize,
                                        t_maximumTextureSize,
                                        r_maximumTextureSize,
                                        resizeToPowerOfTwo);
    }
}

struct ScaleOperator
{
    ScaleOperator():_scale(1.0f) {}
    ScaleOperator(float scale):_scale(scale) {}
    ScaleOperator(const ScaleOperator& so):_scale(so._scale) {}

    ScaleOperator& operator = (const ScaleOperator& so) { _scale = so._scale; return *this; }

    float _scale;

    inline void luminance(float& l) const { l*= _scale; }
    inline void alpha(float& a) const { a*= _scale; }
    inline void luminance_alpha(float& l,float& a) const { l*= _scale; a*= _scale;  }
    inline void rgb(float& r,float& g,float& b) const { r*= _scale; g*=_scale; b*=_scale; }
    inline void rgba(float& r,float& g,float& b,float& a) const { r*= _scale; g*=_scale; b*=_scale; a*=_scale; }
};

void clampToNearestValidPowerOfTwo(int& sizeX, int& sizeY, int& sizeZ, int s_maximumTextureSize, int t_maximumTextureSize, int r_maximumTextureSize)
{
    // compute nearest powers of two for each axis.
    int s_nearestPowerOfTwo = 1;
    while(s_nearestPowerOfTwo<sizeX && s_nearestPowerOfTwo<s_maximumTextureSize) s_nearestPowerOfTwo*=2;

    int t_nearestPowerOfTwo = 1;
    while(t_nearestPowerOfTwo<sizeY && t_nearestPowerOfTwo<t_maximumTextureSize) t_nearestPowerOfTwo*=2;

    int r_nearestPowerOfTwo = 1;
    while(r_nearestPowerOfTwo<sizeZ && r_nearestPowerOfTwo<r_maximumTextureSize) r_nearestPowerOfTwo*=2;

    sizeX = s_nearestPowerOfTwo;
    sizeY = t_nearestPowerOfTwo;
    sizeZ = r_nearestPowerOfTwo;
}

class TestSupportOperation: public osg::GraphicsOperation
{
public:

    TestSupportOperation():
        osg::GraphicsOperation("TestSupportOperation",false),
        supported(true),
        errorMessage(),
        maximumTextureSize(256) {}

    virtual void operator () (osg::GraphicsContext* gc)
    {
        OpenThreads::ScopedLock<OpenThreads::Mutex> lock(mutex);

        glGetIntegerv( GL_MAX_3D_TEXTURE_SIZE, &maximumTextureSize );

        osg::notify(osg::NOTICE)<<"Max texture size="<<maximumTextureSize<<std::endl;
    }

    OpenThreads::Mutex  mutex;
    bool                supported;
    std::string         errorMessage;
    GLint               maximumTextureSize;
};

///////////////////////////////////////////////////////////////////////////////////////////////////
// Python wrapper code.
#ifdef OMEGA_USE_PYTHON
#include "omega/PythonInterpreterWrapper.h"
BOOST_PYTHON_MODULE(myvolume)
{
	// SceneLoader
	PYAPI_REF_BASE_CLASS(myOsgVolume)
		PYAPI_STATIC_REF_GETTER(myOsgVolume, createAndInitialize)
		PYAPI_METHOD(myOsgVolume, setPosition)
		PYAPI_METHOD(myOsgVolume, setRotation)
		PYAPI_METHOD(myOsgVolume, translate)
		PYAPI_METHOD(myOsgVolume, rotate)
		
		PYAPI_METHOD(myOsgVolume, activateEffect)

		PYAPI_METHOD(myOsgVolume, setCustomizedProperty)
		PYAPI_METHOD(myOsgVolume, setClipping)
		PYAPI_METHOD(myOsgVolume, addTransferPoint)
		PYAPI_METHOD(myOsgVolume, clearTransferFunction)
		PYAPI_METHOD(myOsgVolume, setAlphaFunc)
		PYAPI_METHOD(myOsgVolume, setScale)
		PYAPI_METHOD(myOsgVolume, setSampleDensity)
		PYAPI_METHOD(myOsgVolume, setTransparency)
		PYAPI_METHOD(myOsgVolume, setDirty)
		;
		//PYAPI_METHOD(HelloModule, )
		
}
#endif

void myOsgVolume::setPosition( float x, float y, float z)
{
	this->modelForm->setPosition(osg::Vec3f(x, y, z));
}

void myOsgVolume::setRotation( float fx, float fy, float fz, float degree)
{
	this->modelForm->setAttitude(osg::Quat(degree, osg::Vec3f(fx, fy, fz)));
}

void myOsgVolume::translate( float x, float y, float z)
{
	osg::Vec3d pos = this->modelForm->getPosition();
	pos+=osg::Vec3d(x, y, z);
	this->modelForm->setPosition(pos);
}

void myOsgVolume::rotate( float fx, float fy, float fz, float degree)
{
	osg::Quat quat = this->modelForm->getAttitude();
	quat*=osg::Quat(degree,osg::Vec3f(fx, fy, fz));
	this->modelForm->setAttitude(quat);
}

myOsgVolume* myOsgVolume::createAndInitialize(std::string filename, float alpha, float fx, float fy, float fz)
{
	myOsgVolume* instance = new myOsgVolume(filename, alpha, fx, fy, fz);
	ModuleServices::addModule(instance);
	instance->doInitialize(Engine::instance());
	return instance;
}

void myOsgVolume::update(const UpdateContext& context)
{
}

void myOsgVolume::setArguments()
{
	//std::cout << "*********************" << std::endl;
	//imageFile = "C:\\AJ\\CS526\\data\\bmp\\"*;
	//std::cout << imageFile << std::endl;
	//_xScale = 1.0;
	//_yScale = 1.0;
	//_zScale = 1.0;
	//_alpha = 0.02;
	//modelForm = new osg::PositionAttitudeTransform();
	//modelForm->setPosition(osg::Vec3(0,0,0));
}

void myOsgVolume::setAlphaFunc(float alpha)
{
	_alpha = alpha;
	if(_ap)
	{
		_ap->setValue(alpha);
		setDirty();
	}
}

void myOsgVolume::setScale( float x, float y, float z)
{
	_xScale = x;
	_yScale = y;
	_zScale = z;
}

void myOsgVolume::setSampleDensity(float sd)
{
	_sampleDensity = sd;
	if(_sd)
	{
		_sd->setValue(sd);
		setDirty();
	}
}

void myOsgVolume::setTransparency(float tp)
{
	_transparency = tp;
	if(_tp)
	{
		_tp->setValue(tp);
		setDirty();
	}
}

void myOsgVolume::clearTransferFunction()
{
	_tf->clear();
}

void myOsgVolume::addTransferPoint(float intensity, float r, float g, float b, float alpha)
{
	_tf->setColor(intensity, osg::Vec4(r, g, b, alpha));
}

void myOsgVolume::setDirty()
{
	_volumeTile->setDirty(true);
}


void myOsgVolume::setCustomizedProperty()
{
}

void myOsgVolume::setClipping()
{
	_volumeTile->setLocator(new osgVolume::Locator(osg::Matrix::translate(0.5, 0, 0)*osg::Matrix::rotate(osg::Quat(0.2, osg::Vec3f(0,1,0)))*osg::Matrix::scale(0.5,0.5,0.5)* (*_matrix)));
	setDirty();
}

void myOsgVolume::activateEffect(int index)
{
	//std::cout << index << " " << _volumeTile->getDirty() << std::endl;
	switch(index)
    {
		case(0):	_effectProperty->setActiveProperty(0); break;
        case(1):	_effectProperty->setActiveProperty(1); break;
        case(2):	_effectProperty->setActiveProperty(2); break;
        case(3):	_effectProperty->setActiveProperty(3); break;
		default:	std::cout<< "out of range" << std::endl; break;
    }
	//std::cout << "And: " << _volumeTile->getDirty() << std::endl;
	//_imageLayer->dirty();
	setDirty();

}


void myOsgVolume::initialize()
{
	//this->setArguments();
	int argcT= 1;
	char buffer[10] = "osgvolume";
	char* q = buffer;
	char* argvT[10];
	*argvT = buffer;
	std::cout << "************ test********* " << argvT[0] << std::endl;
	osg::ArgumentParser arguments(&argcT, argvT);

	osg::ref_ptr<osg::TransferFunction1D> transferFunction;
	if(true)
    {
        transferFunction = new osg::TransferFunction1D;
        transferFunction->setColor(0.0, osg::Vec4(1.0,1.0,1.0,0.0));
        transferFunction->setColor(1.0, osg::Vec4(1.0,1.0,1.0,1.0));
    }
	_tf = transferFunction;

	float xMultiplier = this->_xScale;
	float yMultiplier = this->_yScale;
	float zMultiplier = this->_zScale;
	float alphaFunc = this->_alpha;
    
	ShadingModel shadingModel = MaximumIntensityProjection;

    float xSize=0.0f, ySize=0.0f, zSize=0.0f;
    
    osg::ref_ptr<TestSupportOperation> testSupportOperation = new TestSupportOperation;


    int maximumTextureSize = testSupportOperation->maximumTextureSize;
    int s_maximumTextureSize = maximumTextureSize;
    int t_maximumTextureSize = maximumTextureSize;
    int r_maximumTextureSize = maximumTextureSize;
    while(arguments.read("--maxTextureSize",maximumTextureSize))
    {
        s_maximumTextureSize = maximumTextureSize;
        t_maximumTextureSize = maximumTextureSize;
        r_maximumTextureSize = maximumTextureSize;
    }
    while(arguments.read("--s_maxTextureSize",s_maximumTextureSize)) {}
    while(arguments.read("--t_maxTextureSize",t_maximumTextureSize)) {}
    while(arguments.read("--r_maxTextureSize",r_maximumTextureSize)) {}

    // set up colour space operation.
    osg::ColorSpaceOperation colourSpaceOperation = osg::NO_COLOR_SPACE_OPERATION;
    osg::Vec4 colourModulate(0.25f,0.25f,0.25f,0.25f);
    while(arguments.read("--modulate-alpha-by-luminance")) { colourSpaceOperation = osg::MODULATE_ALPHA_BY_LUMINANCE; }
    while(arguments.read("--modulate-alpha-by-colour", colourModulate.x(),colourModulate.y(),colourModulate.z(),colourModulate.w() )) { colourSpaceOperation = osg::MODULATE_ALPHA_BY_COLOR; }
    while(arguments.read("--replace-alpha-with-luminance")) { colourSpaceOperation = osg::REPLACE_ALPHA_WITH_LUMINANCE; }
    while(arguments.read("--replace-rgb-with-luminance")) { colourSpaceOperation = osg::REPLACE_RGB_WITH_LUMINANCE; }


    enum RescaleOperation
    {
        NO_RESCALE,
        RESCALE_TO_ZERO_TO_ONE_RANGE,
        SHIFT_MIN_TO_ZERO
    };

    RescaleOperation rescaleOperation = RESCALE_TO_ZERO_TO_ONE_RANGE;

    bool resizeToPowerOfTwo = false;

    unsigned int numComponentsDesired = 0;
    while(arguments.read("--num-components", numComponentsDesired)) {}

    bool useManipulator = false;
    
    bool useShader = true;
    
    bool gpuTransferFunction = true;
    
    double sampleDensityWhenMoving = 0.0;
    
    double sequenceLength = 10.0;
    
    typedef std::list< osg::ref_ptr<osg::Image> > Images;
    Images images;

	osg::ImageList imageList;
	if (imageFile.length() > 0)
    {
		std::string arg = imageFile;
        if (arg.find('*') != std::string::npos)
        {
            osgDB::DirectoryContents contents = osgDB::expandWildcardsInFilename(arg);
            for (unsigned int i = 0; i < contents.size(); ++i)
            {
                osg::Image *image = osgDB::readImageFile( contents[i] );

                if(image)
                {
                    OSG_NOTICE<<"Read osg::Image FileName::"<<image->getFileName()<<", pixelFormat=0x"<<std::hex<<image->getPixelFormat()<<std::dec<<", s="<<image->s()<<", t="<<image->t()<<", r="<<image->r()<<std::endl;
                    imageList.push_back(image);
                }
            }
        }
        else
        {
            // not an option so assume string is a filename.
            osg::Image *image = osgDB::readImageFile( arg );

            if(image)
            {
                OSG_NOTICE<<"Read osg::Image FileName::"<<image->getFileName()<<", pixelFormat=0x"<<std::hex<<image->getPixelFormat()<<std::dec<<", s="<<image->s()<<", t="<<image->t()<<", r="<<image->r()<<std::endl;
                imageList.push_back(image);
            }
        }
    }
	
    // pack the textures into a single texture.
    osg::Image* image = createTexture3D(imageList, numComponentsDesired, s_maximumTextureSize, t_maximumTextureSize, r_maximumTextureSize, resizeToPowerOfTwo);
    if (image)
    {
        images.push_back(image);
    }
    else
    {
        OSG_NOTICE<<"Unable to create 3D image from source files."<<std::endl;
    }


    if (images.empty())
    {
        std::cout<<"No model loaded, please specify a volumetric image file on the command line."<<std::endl;
        return;
    }


    Images::iterator sizeItr = images.begin();
    int image_s = (*sizeItr)->s();
    int image_t = (*sizeItr)->t();
    int image_r = (*sizeItr)->r();
    ++sizeItr;

	std::cout << ">>>>>>>>>>>>>>>>>image size>>>>>>>>>>>>>>>>>>>>>>" << std::endl;
	std::cout << image_s << " " << image_t << " " << image_r << std::endl;

    for(;sizeItr != images.end(); ++sizeItr)
    {
        if ((*sizeItr)->s() != image_s ||
            (*sizeItr)->t() != image_t ||
            (*sizeItr)->r() != image_r)
        {
            std::cout<<"Images in sequence are not of the same dimensions."<<std::endl;
            return;
        }
    }


    osg::ref_ptr<osgVolume::ImageDetails> details = dynamic_cast<osgVolume::ImageDetails*>(images.front()->getUserData());
    osg::ref_ptr<osg::RefMatrix> matrix = details ? details->getMatrix() : dynamic_cast<osg::RefMatrix*>(images.front()->getUserData());

    if (!matrix)
    {
        if (xSize==0.0) xSize = static_cast<float>(image_s);
        if (ySize==0.0) ySize = static_cast<float>(image_t);
        if (zSize==0.0) zSize = static_cast<float>(image_r);

        matrix = new osg::RefMatrix(xSize, 0.0,   0.0,   0.0,
                                    0.0,   ySize, 0.0,   0.0,
                                    0.0,   0.0,   zSize, 0.0,
                                    0.0,   0.0,   0.0,   1.0);
    }
	
    if (xMultiplier!=1.0 || yMultiplier!=1.0 || zMultiplier!=1.0)
    {
        matrix->postMultScale(osg::Vec3d(fabs(xMultiplier), fabs(yMultiplier), fabs(zMultiplier)));
    }

	// AJ set the matrix to class variable
	_matrix = matrix;
	
    osg::Vec4 minValue(FLT_MAX, FLT_MAX, FLT_MAX, FLT_MAX);
    osg::Vec4 maxValue(-FLT_MAX, -FLT_MAX, -FLT_MAX, -FLT_MAX);
    bool computeMinMax = false;
    for(Images::iterator itr = images.begin();
        itr != images.end();
        ++itr)
    {
        osg::Vec4 localMinValue, localMaxValue;
        if (osg::computeMinMax(itr->get(), localMinValue, localMaxValue))
        {
            if (localMinValue.r()<minValue.r()) minValue.r() = localMinValue.r();
            if (localMinValue.g()<minValue.g()) minValue.g() = localMinValue.g();
            if (localMinValue.b()<minValue.b()) minValue.b() = localMinValue.b();
            if (localMinValue.a()<minValue.a()) minValue.a() = localMinValue.a();

            if (localMaxValue.r()>maxValue.r()) maxValue.r() = localMaxValue.r();
            if (localMaxValue.g()>maxValue.g()) maxValue.g() = localMaxValue.g();
            if (localMaxValue.b()>maxValue.b()) maxValue.b() = localMaxValue.b();
            if (localMaxValue.a()>maxValue.a()) maxValue.a() = localMaxValue.a();

            osg::notify(osg::NOTICE)<<"  ("<<localMinValue<<") ("<<localMaxValue<<") "<<(*itr)->getFileName()<<std::endl;

            computeMinMax = true;
        }
    }

    if (computeMinMax)
    {
        osg::notify(osg::NOTICE)<<"Min value "<<minValue<<std::endl;
        osg::notify(osg::NOTICE)<<"Max value "<<maxValue<<std::endl;

        float minComponent = minValue[0];
        minComponent = osg::minimum(minComponent,minValue[1]);
        minComponent = osg::minimum(minComponent,minValue[2]);
        minComponent = osg::minimum(minComponent,minValue[3]);

        float maxComponent = maxValue[0];
        maxComponent = osg::maximum(maxComponent,maxValue[1]);
        maxComponent = osg::maximum(maxComponent,maxValue[2]);
        maxComponent = osg::maximum(maxComponent,maxValue[3]);

    }


    if (colourSpaceOperation!=osg::NO_COLOR_SPACE_OPERATION)
    {
        for(Images::iterator itr = images.begin();
            itr != images.end();
            ++itr)
        {
            (*itr) = osg::colorSpaceConversion(colourSpaceOperation, itr->get(), colourModulate);
        }
    }

    osg::ref_ptr<osg::Image> image_3d = 0;

    if (images.size()==1)
    {
        osg::notify(osg::NOTICE)<<"Single image "<<images.size()<<" volumes."<<std::endl;
        image_3d = images.front();
    }
    else
    {
        osg::notify(osg::NOTICE)<<"Creating sequence of "<<images.size()<<" volumes."<<std::endl;

        osg::ref_ptr<osg::ImageSequence> imageSequence = new osg::ImageSequence;
        imageSequence->setLength(sequenceLength);
        image_3d = imageSequence.get();
        for(Images::iterator itr = images.begin();
            itr != images.end();
            ++itr)
        {
            imageSequence->addImage(itr->get());
        }
        imageSequence->play();
    }

    osg::ref_ptr<osgVolume::Volume> volume = new osgVolume::Volume;
    osg::ref_ptr<osgVolume::VolumeTile> tile = new osgVolume::VolumeTile;
	_volumeTile = tile;
    volume->addChild(tile.get());

    osg::ref_ptr<osgVolume::ImageLayer> layer = new osgVolume::ImageLayer(image_3d.get());
	_imageLayer = layer;

    if (details)
    {
        layer->setTexelOffset(details->getTexelOffset());
        layer->setTexelScale(details->getTexelScale());
    }

    switch(rescaleOperation)
    {
        case(NO_RESCALE):
            break;

        case(RESCALE_TO_ZERO_TO_ONE_RANGE):
        {
            layer->rescaleToZeroToOneRange();
            break;
        }
        case(SHIFT_MIN_TO_ZERO):
        {
            layer->translateMinToZero();
            break;
        }
    };

    if (xMultiplier<0.0 || yMultiplier<0.0 || zMultiplier<0.0)
    {
        layer->setLocator(new osgVolume::Locator(
            osg::Matrix::translate(xMultiplier<0.0 ? -1.0 : 0.0, yMultiplier<0.0 ? -1.0 : 0.0, zMultiplier<0.0 ? -1.0 : 0.0) *
            osg::Matrix::scale(xMultiplier<0.0 ? -1.0 : 1.0, yMultiplier<0.0 ? -1.0 : 1.0, zMultiplier<0.0 ? -1.0 : 1.0) *
            (*matrix)
            ));;
    }
    else
    {
        //layer->setLocator(new osgVolume::Locator(*matrix));
		layer->setLocator(new osgVolume::Locator(*matrix));
    }
	tile->setLocator(new osgVolume::Locator(*matrix));
    
	tile->setLayer(layer.get());

    tile->setEventCallback(new osgVolume::PropertyAdjustmentCallback());

    if (useShader)
    {
		_effectProperty = new osgVolume::SwitchProperty;
		osgVolume::SwitchProperty* sp = _effectProperty;
        sp->setActiveProperty(0);

        _ap = new osgVolume::AlphaFuncProperty(alphaFunc);
        _sd = new osgVolume::SampleDensityProperty(0.005);
        _tp = new osgVolume::TransparencyProperty(1.0);
		_is = new osgVolume::IsoSurfaceProperty(alphaFunc);
        _tfp = transferFunction.valid() ? new osgVolume::TransferFunctionProperty(transferFunction.get()) : 0;
		{
            // Standard
            osgVolume::CompositeProperty* cp = new osgVolume::CompositeProperty;
            cp->addProperty(_ap);
            cp->addProperty(_sd);
            cp->addProperty(_tp);
            if (_tfp) cp->addProperty(_tfp);

            sp->addProperty(cp);
        }

        {
            // Light
            osgVolume::CompositeProperty* cp = new osgVolume::CompositeProperty;
            cp->addProperty(_ap);
            cp->addProperty(_sd);
            cp->addProperty(_tp);
            cp->addProperty(new osgVolume::LightingProperty);
            if (_tfp) cp->addProperty(_tfp);

            sp->addProperty(cp);
        }

        {
            // Isosurface
            osgVolume::CompositeProperty* cp = new osgVolume::CompositeProperty;
            cp->addProperty(_sd);
            cp->addProperty(_tp);
            cp->addProperty(_is);
            if (_tfp) cp->addProperty(_tfp);

            sp->addProperty(cp);
        }

        {
            // MaximumIntensityProjection
            osgVolume::CompositeProperty* cp = new osgVolume::CompositeProperty;
            cp->addProperty(_ap);
            cp->addProperty(_sd);
            cp->addProperty(_tp);
            cp->addProperty(new osgVolume::MaximumIntensityProjectionProperty);
            if (_tfp) cp->addProperty(_tfp);

            sp->addProperty(cp);
        }

        switch(shadingModel)
        {
            case(Standard):                     sp->setActiveProperty(0); break;
            case(Light):                        sp->setActiveProperty(1); break;
            case(Isosurface):                   sp->setActiveProperty(2); break;
            case(MaximumIntensityProjection):   sp->setActiveProperty(3); break;
        }
		
        layer->addProperty(sp);
		
        tile->setVolumeTechnique(new osgVolume::RayTracedTechnique);
    }
    else
    {
        layer->addProperty(new osgVolume::AlphaFuncProperty(alphaFunc));
        tile->setVolumeTechnique(new osgVolume::FixedFunctionTechnique);
    }
	
	if (volume.valid())
    {
		osg::ref_ptr<osg::Group> group = new osg::Group;
	    osg::ref_ptr<osg::Node> loadedModel;
		osg::PositionAttitudeTransform* shift = new osg::PositionAttitudeTransform;
		shift->setPosition(osg::Vec3f( -0.5*_xScale*image_s , -0.5*_yScale*image_t, -0.5*_zScale*image_r ));

		myClipNode = new osg::ClipNode;
		osg::ClipPlane * clipPlane = new osg::ClipPlane;

		osg::Plane * plane = new osg::Plane(osg::Vec3d(0,1,0), osg::Vec3d(0,0,0));
		clipPlane->setClipPlane(*plane);

		myClipNode->addClipPlane(clipPlane);
		loadedModel = myClipNode;
		myClipNode->addChild(group);
		group->addChild(shift);
		shift->addChild(volume.get());
		modelForm = new osg::PositionAttitudeTransform;
		modelForm->setPosition(osg::Vec3(0,0,0));

		modelForm->addChild(loadedModel.get());

		myOsg->setRootNode(modelForm);
		
    }

    return;
}
