#pragma once
#include "TagDefineAtlas3.h"

NS_GAF_BEGIN

class TagDefineAtlas4 : public gaf::TagDefineAtlas3
{
public:
    virtual void read(GAFStream*, GAFAsset*, GAFTimeline*) override;
};

NS_GAF_END