#include "GAFPrecompiled.h"
#include "TagDefineTimeline2.h"
#include "PrimitiveDeserializer.h"
#include "GAFLoader.h"
#include "GAFStream.h"
#include "GAFAsset.h"
#include "GAFTimeline.h"

NS_GAF_BEGIN

TagDefineTimeline2::TagDefineTimeline2(GAFLoader* loader) : TagDefineTimeline(loader)
{
}

void TagDefineTimeline2::read(GAFStream* in, GAFAsset* asset, GAFTimeline* timeline)
{
    unsigned int id = in->readU32();
    unsigned int framesCount = in->readU32();
    cocos2d::Rect aabb;
    cocos2d::Point pivot;

    PrimitiveDeserializer::deserialize(in, &aabb);
    PrimitiveDeserializer::deserialize(in, &pivot);

    GAFTimeline *tl = new GAFTimeline(timeline, id, aabb, pivot, framesCount);

    //////////////////////////////////////////////////////////////////////////

    char hasLinkage = in->readUByte();
    if (hasLinkage)
    {
        std::string temp;
        in->readString(&temp);
        tl->setLinkageName(temp);

        in->readString(&temp);
        tl->setBaseClass(temp);

        in->readString(&temp);
        tl->setOriginClass(temp);
    }

    m_loader->loadTags(in, asset, tl);

    asset->pushTimeline(id, tl);
    if (id == 0)
    {
        asset->setRootTimeline(0u);
    }
}

NS_GAF_END