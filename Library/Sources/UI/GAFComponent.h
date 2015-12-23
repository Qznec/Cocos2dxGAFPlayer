#pragma once
#include "GAFUIMacros.h"

NS_CC_BEGIN
class EventListenerTouchOneByOne;
class Camera;
NS_CC_END

NS_GAF_BEGIN
class GAFObject;

NS_GAF_UI_BEGIN
class EventListenerTouchOneByOne;

class GAFComponent : public cocos2d::Ref
{
public:
    enum class TouchEventType
    {
        BEGAN,
        MOVED,
        ENDED,
        CANCELED
    };

    typedef GAFComponent* GAFComponent_ptr;
    typedef std::function<void(GAFComponent_ptr, TouchEventType)> GAFComponentTouchCallback;
    typedef std::function<void(GAFComponent_ptr)> GAFComponentClickCallback;
    typedef std::function<void(GAFComponent_ptr, int)> GAFComponentEventCallback;

    virtual ~GAFComponent() = 0;

    virtual void addTouchEventListener(const GAFComponentTouchCallback& callback);
    virtual void addClickEventListener(const GAFComponentClickCallback& callback);
    virtual void addEventListener(const GAFComponentEventCallback& callback);

    virtual bool onTouchBegan(cocos2d::Touch* touch, cocos2d::Event* unusedEvent);
    virtual void onTouchMoved(cocos2d::Touch* touch, cocos2d::Event* unusedEvent);
    virtual void onTouchEnded(cocos2d::Touch* touch, cocos2d::Event* unusedEvent);
    virtual void onTouchCancelled(cocos2d::Touch* touch, cocos2d::Event* unusedEvent);

    virtual void setEnabled(bool enabled);
    bool isEnabled() const;

    virtual void setTouchEnabled(bool value);
    bool isTouchEnabled() const;

    virtual void init() {};

protected:
    GAFComponent(GAFObject* view);
    GAFComponent(const GAFComponent &) = delete;
    GAFComponent &operator=(const GAFComponent &) = delete;

    virtual void pushDownEvent();
    virtual void moveEvent();
    virtual void releaseUpEvent();
    virtual void cancelUpEvent();

    virtual bool hitTest(cocos2d::Touch *touch) const;

protected:
    GAFObject* m_view;

    bool m_enabled;
    bool m_touchEnabled;

    bool m_hitted;

    cocos2d::EventListenerTouchOneByOne* m_touchListener;
    cocos2d::EventListenerMouse* m_mouseListener;
    cocos2d::Vec2 m_touchBeganPosition;
    cocos2d::Vec2 m_touchMovePosition;
    cocos2d::Vec2 m_touchEndPosition;

    GAFComponentTouchCallback m_touchEventCallback;
    GAFComponentClickCallback m_clickEventListener;
    GAFComponentEventCallback m_eventCallback;
};

NS_GAF_UI_END
NS_GAF_END