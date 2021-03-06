// -*- C++ -*-
#ifndef V3D_GPU_KLT_H
#define V3D_GPU_KLT_H

# if defined(V3DLIB_GPGPU_ENABLE_CG)

#include "v3d_gpubase.h"
#include "v3d_gpupyramid.h"

namespace V3D_GPU {

struct KLT_TrackerBase {
	KLT_TrackerBase(int nIterations, int nLevels, int levelSkip, int windowWidth) :
			_nIterations(nIterations), _nLevels(nLevels), _levelSkip((levelSkip > 0) ? levelSkip : (nLevels - 1)),
			_windowWidth(windowWidth), _width(0), _height(0), _featureWidth(0), _featureHeight(0), _margin(5),
			_convergenceThreshold(0.01f), _SSD_Threshold(1000.0f) {
			}

			~KLT_TrackerBase() {
			}
			void setBorderMargin(float margin) {
				_margin = margin;
			}
			void setConvergenceThreshold(float thr) {
				_convergenceThreshold = thr;
			}
			void setSSD_Threshold(float thr) {
				_SSD_Threshold = thr;
			}

			void swapFeatureBuffers() {
				std::swap(_featuresBuffer0, _featuresBuffer1);
			}

			unsigned int sourceFeatureTexID() const {
				return _featuresBuffer0->textureID();
			}
			unsigned int targetFeatureTexID() const {
				return _featuresBuffer1->textureID();
			}

			void activateTargetFeatures() {
				_featuresBuffer1->activate();
			}

		protected:
			int const _nIterations, _nLevels, _levelSkip, _windowWidth;
			int _width, _height, _featureWidth, _featureHeight;
			float _margin, _convergenceThreshold, _SSD_Threshold;

			RTT_Buffer *_featuresBuffer0, *_featuresBuffer1;
		};	// end struct KLT_TrackerBase

//----------------------------------------------------------------------

struct KLT_Tracker: public KLT_TrackerBase {
	KLT_Tracker(int nIterations = 5, int nLevels = 4, int levelSkip = -1, int windowWidth = 7) :
			KLT_TrackerBase(nIterations, nLevels, levelSkip, windowWidth), _featuresBufferA("rgb=32f", "KLT_Tracker::_featuresBufferA"), _featuresBufferB("rgb=32f", "KLT_Tracker::_featuresBufferB"), _trackingShader(0) {
		_featuresBuffer0 = &_featuresBufferA;
		_featuresBuffer1 = &_featuresBufferB;
	}

	~KLT_Tracker() {
	}

	void allocate(int width, int height, int featureWidth, int featureHeight);
	void deallocate();

	void provideFeatures(float const * features);
	void readFeatures(float * features);
	void trackFeatures(unsigned int pyrTex0, unsigned int pyrTex1);

protected:
	RTT_Buffer _featuresBufferA, _featuresBufferB;

	Cg_FragmentProgram * _trackingShader;
};
// end struct KLT_Tracker

//----------------------------------------------------------------------

struct KLT_TrackerWithGain: public KLT_TrackerBase {
	KLT_TrackerWithGain(int nIterations = 5, int nLevels = 4, int levelSkip = -1, int windowWidth = 7) :
			KLT_TrackerBase(nIterations, nLevels, levelSkip, windowWidth), _featuresBufferA("rgb=32f", "KLT_TrackerWithGain::_featuresBufferA"), _featuresBufferB("rgb=32f", "KLT_TrackerWithGain::_featuresBufferB"), _featuresBufferC("rgb=32f", "KLT_TrackerWithGain::_featuresBufferC"), _trackingShader(
					0) {
		_featuresBuffer0 = &_featuresBufferA;
		_featuresBuffer1 = &_featuresBufferB;
		_featuresBuffer2 = &_featuresBufferC;
	}

	~KLT_TrackerWithGain() {
	}

	void allocate(int width, int height, int featureWidth, int featureHeight);
	void deallocate();

	void provideFeaturesAndGain(float const * features);
	void readFeaturesAndGain(float * features);
	void trackFeaturesAndGain(unsigned int pyrTex0, unsigned int pyrTex1);

protected:
	RTT_Buffer _featuresBufferA, _featuresBufferB, _featuresBufferC;

	RTT_Buffer * _featuresBuffer2;

	Cg_FragmentProgram * _trackingShader;
};
// end struct KLT_TrackerWithGain

//----------------------------------------------------------------------

struct KLT_Detector {
	KLT_Detector(int minDist = 5) :
			_minDist(minDist), _margin(10.0f), _convRowsBuffer("rgb=32f", "KLT_Detector::_convRowsBuffer"), _cornernessBuffer("rgba=8", "KLT_Detector::_cornernessBuffer"), _nonmaxRowsBuffer("rgba=8", "KLT_Detector::_nonmaxRowsBuffer"), _pointListBuffer("rgb=32f", "KLT_Detector::_pointListBuffer"), _cornernessShader1(
					0), _cornernessShader2(0), _nonmaxShader(0), _traverseShader(0) {
	}

	~KLT_Detector() {
		//by tsou
		if (_cornernessShader1)
			delete _cornernessShader1;
		if (_cornernessShader2)
			delete _cornernessShader2;
		if (_nonmaxShader)
			delete _nonmaxShader;
		if (_traverseShader)
			delete _traverseShader;
	}

	void setBorderMargin(float margin) {
		_margin = margin;
	}

	void allocate(int width, int height, int pointListWidth, int pointListHeight);
	void deallocate();

	// Note: the texID should be a texture as generated by GPUPyramidWithDerivativesCreator.
	void detectCorners(float const minCornerness, unsigned int texID, int& nFeatures, int nPresentFeatures = 0, float * presentFeaturesBuffer = 0);
	void extractCorners(int const nFeatures, float * dest);

protected:
	int const _minDist;

	int _width, _height;
	int _pointListWidth, _pointListHeight;
	int _histpyrWidth, _nHistpyrLevels;

	float _margin;

	RTT_Buffer _convRowsBuffer; // temporary buffer for structure matrix accumulation
	RTT_Buffer _cornernessBuffer;
	RTT_Buffer _nonmaxRowsBuffer; // temporary buffer for non-max suppression
	RTT_Buffer _pointListBuffer;

	unsigned int _histpyrTexID;
	unsigned int _histpyrFbIDs[16];
	unsigned int _vbo;

	Cg_FragmentProgram * _cornernessShader1;
	Cg_FragmentProgram * _cornernessShader2;
	Cg_FragmentProgram * _nonmaxShader;
	Cg_FragmentProgram * _traverseShader;
};
// end struct KLT_Detector

struct KLT_TrackedFeature {
	KLT_TrackedFeature() :
			status(-1), gain(1.0f), fed(-1) {
	}

	//! 0 means tracked from previous frame, 1 is newly created and -1 means invalidated track.
	int status;
	float pos[2];
	float gain;
	int fed; // >=0 is the id of a feature point fed to tracking
};

//----------------------------------------------------------------------

struct KLT_SequenceTrackerConfig {
	KLT_SequenceTrackerConfig() :
			nIterations(12), //!< #Iterations in the tracker
			nLevels(3), //!< Levels used in the coarse-to-fine approach
			levelSkip(2), //!< Levels skipped, reasonable values are 1 (all levels) and nLevels-1 (2 levels)
			windowWidth(5), //!< Size of the floating window
			trackBorderMargin(4.0f), //!< In pixels
			convergenceThreshold(0.1f), //!< In pixels
			SSD_Threshold(5000.0f), trackWithGain(false), minDistance(8), //!< Radius of non-max feature suppression
			minCornerness(1000.0f), detectBorderMargin(4.0f) //!< In pixels
	{
	}

	int nIterations, nLevels, levelSkip, windowWidth;
	float trackBorderMargin, convergenceThreshold, SSD_Threshold;
	bool trackWithGain;

	int minDistance;
	float minCornerness, detectBorderMargin;
};
// end struct KLT_SequenceTrackerConfig

struct KLT_SequenceTracker {
	KLT_SequenceTracker(KLT_SequenceTrackerConfig const& config) :
			_config(config), _trackWithGain(config.trackWithGain), _detector(config.minDistance), _tracker(0), _trackerWithGain(0) {
		_pyrCreator0 = &_pyrCreatorA;
		_pyrCreator1 = &_pyrCreatorB;
		_corners = 0;
	}

	~KLT_SequenceTracker() {
	}

	void allocate(int width, int height, int nLevels, int featuresWidth, int featuresHeight) {
		this->allocate(width, height, nLevels, featuresWidth, featuresHeight, 2 * featuresWidth, 2 * featuresHeight);
	}
	void allocate(int width, int height, int nLevels, int featuresWidth, int featuresHeight, int pointListWidth, int pointListHeight);
	void deallocate();

	void setBorderMargin(float margin) {
		if (_tracker)
			_tracker->setBorderMargin(margin);
		else
			_trackerWithGain->setBorderMargin(margin);

		_detector.setBorderMargin(margin);
	}

	void setConvergenceThreshold(float thr) {
		if (_tracker)
			_tracker->setConvergenceThreshold(thr);
		else
			_trackerWithGain->setConvergenceThreshold(thr);
	}

	void setSSD_Threshold(float thr) {
		if (_tracker)
			_tracker->setSSD_Threshold(thr);
		else
			_trackerWithGain->setSSD_Threshold(thr);
	}

	void detect(unsigned char const * image, int& nDetectedFeatures, KLT_TrackedFeature * dest);
	//Add by Danping Zou for feeding custom feature points to track
	void detect(unsigned char const * image, int& nDetectedFeatures, KLT_TrackedFeature* dest, int nPresent, float* present);
	void redetect(unsigned char const * image, int& nNewFeatures, KLT_TrackedFeature * dest);

	//Add by Danping Zou for feeding custom feature points to track
	void feedExternFeaturePoints(int npts, float* featPts, int * trackIds, int& nFed);

	void track(unsigned char const * image, int& nPresentFeatures, KLT_TrackedFeature * dest);

	void advanceFrame() {
		if (_tracker)
			_tracker->swapFeatureBuffers();
		else
			_trackerWithGain->swapFeatureBuffers();

		std::swap(_pyrCreator0, _pyrCreator1);
	}

	unsigned int getCurrentFrameTextureID() const {
		return _pyrCreator1->sourceTextureID();
	}

public:
	struct FeaturePoint {
		float data[3];

		bool operator<(FeaturePoint const& b) const {
			return this->data[2] > b.data[2];
		}
	}; // end struct FeaturePoint

	KLT_SequenceTrackerConfig const _config;

	bool const _trackWithGain;

	int _width, _height;
	int _featuresWidth, _featuresHeight;
	int _pointListWidth, _pointListHeight;

	PyramidWithDerivativesCreator _pyrCreatorA;
	PyramidWithDerivativesCreator _pyrCreatorB;
	KLT_Detector _detector;

	KLT_Tracker * _tracker;
	KLT_TrackerWithGain * _trackerWithGain;

	PyramidWithDerivativesCreator * _pyrCreator0;
	PyramidWithDerivativesCreator * _pyrCreator1;

	FeaturePoint * _corners;
};
// end struct KLT_SequenceTracker

}// end namespace V3D_GPU

# endif // defined(V3DLIB_GPGPU_ENABLE_CG)
#endif
