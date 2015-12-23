#include "GAFPrecompiled.h"
#include "GAFComponent.h"
#include "GAFObject.h"
#include "base/CCEventListenerTouch.h"
#include <iostream>

NS_GAF_BEGIN
NS_GAF_UI_BEGIN

GAFComponent::GAFComponent(GAFObject *view)
: m_view(view)
, m_enabled(true)
, m_touchEnabled(false)
, m_hitted(false)
, m_touchListener(nullptr)
, m_touchEventCallback(nullptr)
, m_clickEventListener(nullptr)
, m_eventCallback(nullptr)
{
}

GAFComponent::~GAFComponent()
{
    m_view->getEventDispatcher()->removeEventListener(m_touchListener);
    CC_SAFE_RELEASE_NULL(m_touchListener);
}

void GAFComponent::setTouchEnabled(bool value)
{
    m_touchEnabled = value;
    if (m_touchEnabled)
    {
        m_touchListener = cocos2d::EventListenerTouchOneByOne::create();
        CC_SAFE_RETAIN(m_touchListener);
        m_touchListener->setSwallowTouches(true);
        m_touchListener->onTouchBegan = CC_CALLBACK_2(GAFComponent::onTouchBegan, this);
        m_touchListener->onTouchMoved = CC_CALLBACK_2(GAFComponent::onTouchMoved, this);
        m_touchListener->onTouchEnded = CC_CALLBACK_2(GAFComponent::onTouchEnded, this);
        m_touchListener->onTouchCancelled = CC_CALLBACK_2(GAFComponent::onTouchCancelled, this);
        m_view->getEventDispatcher()->addEventListenerWithSceneGraphPriority(m_touchListener, m_view);
    }
    else
    {
        m_view->getEventDispatcher()->removeEventListener(m_touchListener);
        CC_SAFE_RELEASE_NULL(m_touchListener);
    }
}

bool GAFComponent::isTouchEnabled() const
{
    return m_touchEnabled;
}

bool GAFComponent::onTouchBegan(cocos2d::Touch *touch, cocos2d::Event *unusedEvent)
{
    m_hitted = false;
    if (m_view->isVisible() && isEnabled())
    {
        if (hitTest(touch))
        {
            m_hitted = true;
        }
    }

    if (!m_hitted)
    {
        return false;
    }

    pushDownEvent();
    return true;
}

void GAFComponent::onTouchMoved(cocos2d::Touch *touch, cocos2d::Event *unusedEvent)
{
    m_touchMovePosition = touch->getLocation();

    moveEvent();
}

void GAFComponent::onTouchEnded(cocos2d::Touch *touch, cocos2d::Event *unusedEvent)
{
    if (hitTest(touch))
    {
        releaseUpEvent();
    }
    else
    {
        cancelUpEvent();
    }
}

void GAFComponent::onTouchCancelled(cocos2d::Touch *touch, cocos2d::Event *unusedEvent)
{
    cancelUpEvent();
}

void GAFComponent::setEnabled(bool enabled)
{
    m_enabled = enabled;

    setTouchEnabled(m_enabled);
}

bool GAFComponent::isEnabled() const
{
    return m_enabled;
}

void GAFComponent::pushDownEvent()
{
    this->retain();
    if (m_touchEventCallback)
    {
        m_touchEventCallback(this, TouchEventType::BEGAN);
    }
    this->release();
}

void GAFComponent::moveEvent()
{
    this->retain();
    if (m_touchEventCallback)
    {
        m_touchEventCallback(this, TouchEventType::MOVED);
    }
    this->release();
}

void GAFComponent::releaseUpEvent()
{
    this->retain();
    if (m_touchEventCallback)
    {
        m_touchEventCallback(this, TouchEventType::ENDED);
    }

    if (m_clickEventListener)
    {
        m_clickEventListener(this);
    }
    this->release();
}

void GAFComponent::cancelUpEvent()
{
    this->retain();
    if (m_touchEventCallback)
    {
        m_touchEventCallback(this, TouchEventType::CANCELED);
    }
    this->release();
}

void GAFComponent::addTouchEventListener(const GAFComponentTouchCallback& callback)
{
    this->m_touchEventCallback = callback;
}

void GAFComponent::addClickEventListener(const GAFComponentClickCallback &callback)
{
    this->m_clickEventListener = callback;
}

void GAFComponent::addEventListener(const GAFComponentEventCallback &callback)
{
    this->m_eventCallback = callback;
}

bool GAFComponent::hitTest(cocos2d::Touch *touch) const
{
    cocos2d::Rect rect = m_view->getBoundingBoxForCurrentFrame();
    auto localTouchBeganPosition = m_view->getParent()->convertTouchToNodeSpace(touch);

    return rect.containsPoint(localTouchBeganPosition);
}

NS_GAF_UI_END
NS_GAF_END