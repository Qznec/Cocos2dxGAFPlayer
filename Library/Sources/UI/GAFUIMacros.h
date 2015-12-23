#pragma once

#define NS_GAF_UI_BEGIN    namespace ui {
#define NS_GAF_UI_END      }
#define USING_NS_GAF_UI    USING_NS_GAF.ui

#define GAF_CREATE_COMPONENT_CTORS(TypeName) \
public: \
    static TypeName* create(gaf::GAFObject* view) \
    { \
        TypeName* ret = new (std::nothrow) TypeName(view); \
        if (ret) \
        { \
            ret->autorelease(); \
            ret->init(); \
        } \
        else \
        { \
            CC_SAFE_DELETE(ret); \
        } \
        return ret; \
    } \
protected: \
    TypeName(gaf::GAFObject* view); \
    TypeName(const TypeName &) = delete; \
    TypeName &operator=(const TypeName &) = delete;
