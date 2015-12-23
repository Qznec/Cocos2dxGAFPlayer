#pragma once
#include "GAFComponent.h"

NS_GAF_BEGIN

class GAFObject;

NS_GAF_UI_BEGIN

class GAFButton : public GAFComponent
{
public:
    enum class State : uint8_t
    {
        STATE_NONE = 0,
        STATE_OVER = 1,
        STATE_DOWN = 2,
        STATE_DISABLED = 3,
        STATE_SELECTED = 4,
        STATE_SELECTED_OVER = 5,
        STATE_SELECTED_DOWN = 6,
        STATE_SELECTED_DISABLED = 7
    };

protected:
    static const char STATE_NONE_STR[];
    static const char STATE_OVER_STR[];
    static const char STATE_DOWN_STR[];
    static const char STATE_DISABLED_STR[];
    static const char STATE_SELECTED_STR[];
    static const char STATE_SELECTED_OVER_STR[];
    static const char STATE_SELECTED_DOWN_STR[];
    static const char STATE_SELECTED_DISABLED_STR[];

    static const char GAFButton::HIT_OBJECT_NAME[];

    bool m_selected;
    bool m_checkBoxMode;

    cocos2d::Rect m_hitZone;

    std::map<State, uint32_t> m_frames;

    GAF_CREATE_COMPONENT_CTORS(GAFButton)

public:
    virtual ~GAFButton() override;

    virtual void init() override;

    bool isSelected() const { return m_selected; }
    virtual void setSelected(bool value);

    bool isCheckBoxMode() const { return m_checkBoxMode; }
    virtual void setCheckBoxMode(bool value);

    virtual void setEnabled(bool value) override;

protected:
    /**
    * This function helps artists and removes necessity to make frame labels. <br>
    * 1-3 frame IDs are the same as in SimpleButton, that allows you to convert <br>
    * SimpleButtons to MovieClips in IDE without necessity to make frame labels.
    */
    void checkFrameLabels();
    uint32_t getFrameByLabel(const std::string& name);
    void changeFrame(State state);

    virtual void pushDownEvent() override;
    virtual void moveEvent() override;
    virtual void releaseUpEvent() override;
    virtual void cancelUpEvent() override;

    virtual bool hitTest(cocos2d::Touch *touch) const override;
};

NS_GAF_UI_END
NS_GAF_END