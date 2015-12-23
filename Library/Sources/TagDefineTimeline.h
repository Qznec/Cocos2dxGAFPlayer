#pragma once

#include "DefinitionTagBase.h"

NS_GAF_BEGIN

class GAFLoader;

class TagDefineTimeline : public DefinitionTagBase
{
protected:
    GAFLoader*  m_loader; // weak
public:

    TagDefineTimeline(GAFLoader*);

    virtual void read(GAFStream*, GAFAsset*, GAFTimeline*) override;

};

NS_GAF_END