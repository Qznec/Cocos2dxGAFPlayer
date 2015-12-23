#pragma once

#include "GAFObject.h"
#include "GAFTextData.h"

NS_GAF_BEGIN

class GAFTextField : public GAFObject
{
protected:
    cocos2d::Label *m_label;

public:
    GAFTextField();
    virtual ~GAFTextField();

    void initWithTextData(const GAFTextData *data);

    void setText(std::string text);

    virtual cocos2d::Rect getBoundingBox() const override;
};

NS_GAF_END