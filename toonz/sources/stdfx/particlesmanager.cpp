

#include "trenderer.h"

#include <QMutexLocker>

#include "particlesmanager.h"

/*
EXPLANATION:

ParticlesManager improves the old particles system as follows - the particles manager stores the
last particles configuration which had some particle rendered by a thread.
Under normal cicumstances, this means that every thread has the particles configuration that rendered
last. In case a trail was set, such frame is that beyond the trail.
This managemer works well on the assumption that each thread builds particle in an incremental timeline.
*/

//--------------------------------------------------------------------------------------------------

DEFINE_CLASS_CODE(ParticlesManager::FxData, 110)

//--------------------------------------------------------------------------------------------------

typedef std::map<double, ParticlesManager::FrameData> FramesMap;

//************************************************************************************************
//    Preliminaries
//************************************************************************************************

class ParticlesManagerGenerator : public TRenderResourceManagerGenerator
{
public:
	ParticlesManagerGenerator() : TRenderResourceManagerGenerator(true) {}

	TRenderResourceManager *operator()(void)
	{
		return new ParticlesManager;
	}
};

MANAGER_FILESCOPE_DECLARATION(ParticlesManager, ParticlesManagerGenerator);

//************************************************************************************************
//    FrameData implementation
//************************************************************************************************

ParticlesManager::FrameData::FrameData(FxData *fxData)
	: m_fxData(fxData), m_frame((std::numeric_limits<int>::min)()), m_calculated(false), m_maxTrail(-1), m_totalParticles(0)
{
	m_fxData->addRef();
}

//-------------------------------------------------------------------------

ParticlesManager::FrameData::~FrameData()
{
	m_fxData->release();
}

//-------------------------------------------------------------------------

void ParticlesManager::FrameData::buildMaxTrail()
{
	//Store the maximum trail of each particle
	std::list<Particle>::iterator it;
	for (it = m_particles.begin(); it != m_particles.end(); ++it)
		m_maxTrail = tmax(m_maxTrail, it->trail);
}

//-------------------------------------------------------------------------

void ParticlesManager::FrameData::clear()
{
	m_frame = (std::numeric_limits<int>::min)();
	m_particles.clear();
	m_random = TRandom();
	m_calculated = false;
	m_maxTrail = -1;
	m_totalParticles = 0;
}

//************************************************************************************************
//    FxData implementation
//************************************************************************************************

ParticlesManager::FxData::FxData()
	: TSmartObject(m_classCode)
{
}

//************************************************************************************************
//    ParticlesContainer implementation
//************************************************************************************************

ParticlesManager *ParticlesManager::instance()
{
	return static_cast<ParticlesManager *>(
		ParticlesManager::gen()->getManager(TRenderer::renderId()));
}

//-------------------------------------------------------------------------

ParticlesManager::ParticlesManager()
	: m_renderStatus(-1)
{
}

//-------------------------------------------------------------------------

ParticlesManager::~ParticlesManager()
{
	//Release all fxDatas
	std::map<unsigned long, FxData *>::iterator it, end = m_fxs.end();
	for (it = m_fxs.begin(); it != end; ++it)
		it->second->release();
}

//-------------------------------------------------------------------------

void ParticlesManager::onRenderStatusStart(int renderStatus)
{
	m_renderStatus = renderStatus;
}

//-------------------------------------------------------------------------

ParticlesManager::FrameData *ParticlesManager::data(unsigned long fxId)
{
	QMutexLocker locker(&m_mutex);

	std::map<unsigned long, FxData *>::iterator it = m_fxs.find(fxId);
	if (it == m_fxs.end()) {
		it = m_fxs.insert(std::make_pair(fxId, new FxData)).first;
		it->second->addRef();
	}

	FxData *fxData = it->second;
	FrameData *d = fxData->m_frames.localData();
	if (!d) {
		d = new FrameData(fxData);
		fxData->m_frames.setLocalData(d);
	}

	return d;
}