#include "GAFPrecompiled.h"
#include "GAFSprite.h"
#include "GAFCollections.h"

#include "math/TransformUtils.h"
#include "../external/xxhash/xxhash.h"

USING_NS_CC;

#if CC_SPRITEBATCHNODE_RENDER_SUBPIXEL
#define RENDER_IN_SUBPIXEL
#else
#define RENDER_IN_SUBPIXEL(__A__) ( (int)(__A__))
#endif

NS_GAF_BEGIN

static unsigned short scale9quadIndices[] = { 0,1,2, 3,2,1, 4,5,6, 7,6,5, 8,9,10, 11,10,9, 12,13,14, 15,14,13 };
static V3F_C4B_T2F_Quad helperQuads[9] = {};

static Rect intersectRect(const Rect &first, const Rect &second)
{
	Rect ret;
	ret.origin.x = std::max(first.origin.x, second.origin.x);
	ret.origin.y = std::max(first.origin.y, second.origin.y);

	float rightRealPoint = std::min(first.origin.x + first.size.width,
		second.origin.x + second.size.width);
	float bottomRealPoint = std::min(first.origin.y + first.size.height,
		second.origin.y + second.size.height);

	ret.size.width = std::max(rightRealPoint - ret.origin.x, 0.0f);
	ret.size.height = std::max(bottomRealPoint - ret.origin.y, 0.0f);
	return ret;
}

static void updateQuadCoords(V3F_C4B_T2F_Quad &quad, const Rect &rect)
{
	quad.tl.vertices.set(rect.origin.x, rect.origin.y, 0.f);
	quad.tr.vertices.set(rect.origin.x + rect.size.width, rect.origin.y, 0.f);
	quad.bl.vertices.set(rect.origin.x, rect.origin.y + rect.size.height, 0.f);
	quad.br.vertices.set(rect.origin.x + rect.size.width, rect.origin.y + rect.size.height, 0.f);
}

static void setQuadUVs(V3F_C4B_T2F_Quad &quad, const Rect &rect,
	const GAFRotation rotation = GAFRotation::NONE,
	bool flippedX = false, bool flippedY = false)
{
	float left = rect.origin.x;
	float right = rect.origin.x + rect.size.width;
	float top = rect.origin.y;
	float bottom = rect.origin.y + rect.size.height;

	switch (rotation)
	{
	case gaf::GAFRotation::CCW_90:
	{
		if (flippedX)
			std::swap(top, bottom);

		if (flippedY)
			std::swap(left, right);

		quad.bl.texCoords.u = right;
		quad.bl.texCoords.v = bottom;
		quad.br.texCoords.u = right;
		quad.br.texCoords.v = top;
		quad.tl.texCoords.u = left;
		quad.tl.texCoords.v = bottom;
		quad.tr.texCoords.u = left;
		quad.tr.texCoords.v = top;
	}
	break;

	case gaf::GAFRotation::CW_90:
	{
		if (flippedX)
			std::swap(top, bottom);

		if (flippedY)
			std::swap(left, right);

		quad.bl.texCoords.u = left;
		quad.bl.texCoords.v = top;
		quad.br.texCoords.u = left;
		quad.br.texCoords.v = bottom;
		quad.tl.texCoords.u = right;
		quad.tl.texCoords.v = top;
		quad.tr.texCoords.u = right;
		quad.tr.texCoords.v = bottom;
	}
	break;

	case gaf::GAFRotation::NONE:
	default:
	{
		if (flippedX)
			std::swap(left, right);

		if (flippedY)
			std::swap(top, bottom);

		quad.bl.texCoords.u = left;
		quad.bl.texCoords.v = bottom;
		quad.br.texCoords.u = right;
		quad.br.texCoords.v = bottom;
		quad.tl.texCoords.u = left;
		quad.tl.texCoords.v = top;
		quad.tr.texCoords.u = right;
		quad.tr.texCoords.v = top;
	}
	break;
	}
}

template<typename InputIterator>
typename std::enable_if<std::is_convertible<typename std::iterator_traits<InputIterator>::value_type, cocos2d::V3F_C4B_T2F_Quad*>::value, void>::type
Scale9PolygonInfo::setQuads(InputIterator first, InputIterator last)
{
	releaseVertsAndIndices();
	isVertsOwner = true;

	typename std::iterator_traits<InputIterator>::difference_type n = std::distance(first, last);
	size_t vertCount = n * 4;
	size_t indexCount = n * 6;

	triangles.verts = new V3F_C4B_T2F[vertCount];
	triangles.indices = new unsigned short[indexCount];
	triangles.vertCount = vertCount;
	triangles.indexCount = indexCount;

	const size_t quadVertCount = 4;

	for (auto i = 0; first != last; ++i, ++first)
	{
		V3F_C4B_T2F* verts = reinterpret_cast<V3F_C4B_T2F*>(*first);
		memcpy(triangles.verts + (i * quadVertCount), verts, quadVertCount * sizeof(V3F_C4B_T2F));
	}

	memcpy(triangles.indices, scale9quadIndices, indexCount * sizeof(scale9quadIndices[0]));
}

void Scale9PolygonInfo::releaseVertsAndIndices()
{
	if (isVertsOwner)
	{
		if (nullptr != triangles.verts)
		{
			CC_SAFE_DELETE_ARRAY(triangles.verts);
		}

		if (nullptr != triangles.indices)
		{
			CC_SAFE_DELETE_ARRAY(triangles.indices);
		}
	}
}

GAFSprite::GAFSprite()
: objectIdRef(IDNONE)
, m_externalTransform(AffineTransform::IDENTITY)
, m_scale9Enabled(false)
, m_capInsets(CCRect::ZERO)
, m_isLocator(false)
, m_useSeparateBlendFunc(false)
, m_blendEquation(-1)
, m_atlasScale(1.0f)
, m_rotation(GAFRotation::NONE)
{
#if COCOS2D_VERSION < 0x00030300
    _batchNode = nullptr; // this will fix a bug in cocos2dx 3.2 tag
#endif
    setFlippedX(false); // Fix non-inited vars in cocos
    setFlippedY(false);
    _rectRotated = false;
}

GAFSprite::~GAFSprite()
{
}

void GAFSprite::updateScale9GridQuads()
{
	V3F_C4B_T2F_Quad &tlq = m_scale9Slices[0];
	V3F_C4B_T2F_Quad &tcq = m_scale9Slices[1];
	V3F_C4B_T2F_Quad &trq = m_scale9Slices[2];
	V3F_C4B_T2F_Quad &clq = m_scale9Slices[3];
	V3F_C4B_T2F_Quad &ccq = m_scale9Slices[4];
	V3F_C4B_T2F_Quad &crq = m_scale9Slices[5];
	V3F_C4B_T2F_Quad &blq = m_scale9Slices[6];
	V3F_C4B_T2F_Quad &bcq = m_scale9Slices[7];
	V3F_C4B_T2F_Quad &brq = m_scale9Slices[8];

	float width = _rect.size.width;
	float height = _rect.size.height;

	// If there is no specified center region
	if (m_capInsets.equals(Rect::ZERO))
	{
		// log("... cap insets not specified : using default cap insets ...");
		m_capInsets = Rect(width / 3, height / 3, width / 3, height / 3);
	}

	Rect originalRect = _rect;

	float leftWidth = m_capInsets.origin.x;
	float centerWidth = m_capInsets.size.width;
	float rightWidth = originalRect.size.width - (leftWidth + centerWidth);

	float topHeight = m_capInsets.origin.y;
	float centerHeight = m_capInsets.size.height;
	float bottomHeight = originalRect.size.height - (topHeight + centerHeight);

	// calculate rects

	// ... top row
	float x = 0.0;
	float y = 0.0;

	// top left
	Rect leftTopBoundsOriginal = Rect(x, y, leftWidth, topHeight);
	Rect leftTopBounds = leftTopBoundsOriginal;

	// top center
	x += leftWidth;
	Rect centerTopBounds = Rect(x, y, centerWidth, topHeight);

	// top right
	x += centerWidth;
	Rect rightTopBounds = Rect(x, y, rightWidth, topHeight);

	// ... center row
	x = 0.0;
	y = 0.0;
	y += topHeight;

	// center left
	Rect leftCenterBounds = Rect(x, y, leftWidth, centerHeight);

	// center center
	x += leftWidth;
	Rect centerBoundsOriginal = Rect(x, y, centerWidth, centerHeight);
	Rect centerBounds = centerBoundsOriginal;

	// center right
	x += centerWidth;
	Rect rightCenterBounds = Rect(x, y, rightWidth, centerHeight);

	// ... bottom row
	x = 0.0;
	y = 0.0;
	y += topHeight;
	y += centerHeight;

	// bottom left
	Rect leftBottomBounds = Rect(x, y, leftWidth, bottomHeight);

	// bottom center
	x += leftWidth;
	Rect centerBottomBounds = Rect(x, y, centerWidth, bottomHeight);

	// bottom right
	x += centerWidth;
	Rect rightBottomBoundsOriginal = Rect(x, y, rightWidth, bottomHeight);
	Rect rightBottomBounds = rightBottomBoundsOriginal;

	if ((m_capInsets.origin.x + m_capInsets.size.width) <= width
		|| (m_capInsets.origin.y + m_capInsets.size.height) <= width)
		//in general case it is error but for legacy support we will check it
	{
		leftTopBounds = intersectRect(leftTopBounds, originalRect);
		centerTopBounds = intersectRect(centerTopBounds, originalRect);
		rightTopBounds = intersectRect(rightTopBounds, originalRect);
		leftCenterBounds = intersectRect(leftCenterBounds, originalRect);
		centerBounds = intersectRect(centerBounds, originalRect);
		rightCenterBounds = intersectRect(rightCenterBounds, originalRect);
		leftBottomBounds = intersectRect(leftBottomBounds, originalRect);
		centerBottomBounds = intersectRect(centerBottomBounds, originalRect);
		rightBottomBounds = intersectRect(rightBottomBounds, originalRect);
	}
	else
		//it is error but for legacy turn off clip system
		CCLOG("Scale9Sprite capInsetsInternal > originalSize");

	AffineTransform t = AffineTransform::IDENTITY;
	t = AffineTransformTranslate(t, originalRect.origin.x, originalRect.origin.y);

	leftTopBoundsOriginal = RectApplyAffineTransform(leftTopBoundsOriginal, t);
	centerBoundsOriginal = RectApplyAffineTransform(centerBoundsOriginal, t);
	rightBottomBoundsOriginal = RectApplyAffineTransform(rightBottomBoundsOriginal, t);

	centerBounds = RectApplyAffineTransform(centerBounds, t);
	rightBottomBounds = RectApplyAffineTransform(rightBottomBounds, t);
	leftBottomBounds = RectApplyAffineTransform(leftBottomBounds, t);
	rightTopBounds = RectApplyAffineTransform(rightTopBounds, t);
	leftTopBounds = RectApplyAffineTransform(leftTopBounds, t);
	rightCenterBounds = RectApplyAffineTransform(rightCenterBounds, t);
	leftCenterBounds = RectApplyAffineTransform(leftCenterBounds, t);
	centerBottomBounds = RectApplyAffineTransform(centerBottomBounds, t);
	centerTopBounds = RectApplyAffineTransform(centerTopBounds, t);

	m_topLeftSize = leftTopBoundsOriginal.size;
	m_centerSize = centerBoundsOriginal.size;
	m_bottomRightSize = rightBottomBoundsOriginal.size;

	float offsetX = (centerBounds.origin.x + centerBounds.size.width / 2)
		- (centerBoundsOriginal.origin.x + centerBoundsOriginal.size.width / 2);
	float offsetY = (centerBoundsOriginal.origin.y + centerBoundsOriginal.size.height / 2)
		- (centerBounds.origin.y + centerBounds.size.height / 2);
	m_centerOffset.x = offsetX;
	m_centerOffset.y = offsetY;

	// Centre
	if (centerBounds.size.width > 0 && centerBounds.size.height > 0)
	{
		updateQuadCoords(ccq, centerBounds);
	}

	// Top
	if (centerTopBounds.size.width > 0 && centerTopBounds.size.height > 0)
	{
		updateQuadCoords(tcq, centerTopBounds);
	}

	// Bottom
	if (centerBottomBounds.size.width > 0 && centerBottomBounds.size.height > 0)
	{
		updateQuadCoords(bcq, centerBottomBounds);
	}

	// Left
	if (leftCenterBounds.size.width > 0 && leftCenterBounds.size.height > 0)
	{
		updateQuadCoords(clq, leftCenterBounds);
	}

	// Right
	if (rightCenterBounds.size.width > 0 && rightCenterBounds.size.height > 0)
	{
		updateQuadCoords(crq, rightCenterBounds);
	}

	// Top left
	if (leftTopBounds.size.width > 0 && leftTopBounds.size.height > 0)
	{
		updateQuadCoords(tlq, leftTopBounds);
	}

	// Top right
	if (rightTopBounds.size.width > 0 && rightTopBounds.size.height > 0)
	{
		updateQuadCoords(trq, rightTopBounds);
	}

	// Bottom left
	if (leftBottomBounds.size.width > 0 && leftBottomBounds.size.height > 0)
	{
		updateQuadCoords(blq, leftBottomBounds);
	}

	// Bottom right
	if (rightBottomBounds.size.width > 0 && rightBottomBounds.size.height > 0)
	{
		updateQuadCoords(brq, rightBottomBounds);
	}
}

bool GAFSprite::initWithSpriteFrame(cocos2d::SpriteFrame *spriteFrame, GAFRotation rotation)
{
    m_rotation = rotation;
    return initWithSpriteFrame(spriteFrame);
}

bool GAFSprite::initWithSpriteFrame(cocos2d::SpriteFrame *spriteFrame, GAFRotation rotation, const cocos2d::Rect& capInsets)
{
	m_capInsets = capInsets;

	m_scale9Enabled = !m_capInsets.equals(CCRect::ZERO);

	bool bRet = initWithSpriteFrame(spriteFrame, rotation);

	if (m_scale9Enabled)
		updateScale9GridQuads();

	return bRet;
}

bool GAFSprite::initWithSpriteFrame(cocos2d::SpriteFrame *spriteFrame)
{
    CCASSERT(spriteFrame != nullptr, "");

    bool bRet = cocos2d::Sprite::initWithTexture(spriteFrame->getTexture(), spriteFrame->getRect());
    setSpriteFrame(spriteFrame);

    return bRet;
}

bool GAFSprite::initWithTexture(cocos2d::Texture2D *pTexture, const cocos2d::Rect& rect, bool rotated)
{
    if (cocos2d::Sprite::initWithTexture(pTexture, rect, rotated))
    {
        setGLProgram(cocos2d::GLProgramCache::getInstance()->getGLProgram(cocos2d::GLProgram::SHADER_NAME_POSITION_TEXTURE_COLOR));
        return true;
    }
    else
    {
        return false;
    }
}

bool GAFSprite::initWithTexture(cocos2d::Texture2D *pTexture, const cocos2d::Rect& rect, bool rotated, const cocos2d::Rect& capInsets)
{
	m_capInsets = capInsets;

	m_scale9Enabled = !m_capInsets.equals(CCRect::ZERO);

	bool bRet = initWithTexture(pTexture, rect, rotated);

	if (m_scale9Enabled)
		updateScale9GridQuads();

	return bRet;
}

void GAFSprite::setTexture(cocos2d::Texture2D *texture)
{
    // If batchnode, then texture id should be the same
    CCAssert(!_batchNode || texture->getName() == _batchNode->getTexture()->getName(), "cocos2d::Sprite: Batched sprites should use the same texture as the batchnode");
    // accept texture==nil as argument
    CCAssert(!texture || dynamic_cast<cocos2d::Texture2D*>(texture), "setTexture expects a cocos2d::Texture2D. Invalid argument");

    if (!_batchNode && _texture != texture)
    {
        CC_SAFE_RETAIN(texture);
        CC_SAFE_RELEASE(_texture);
        _texture = texture;
        updateBlendFunc();
    }
}

void GAFSprite::setVertexRect(const cocos2d::Rect& rect)
{
    _rect = rect;
    if (m_rotation != GAFRotation::NONE)
    {
        std::swap(_rect.size.width, _rect.size.height);
    }
}

void GAFSprite::setTextureRect(const cocos2d::Rect& rect, bool rotated, const cocos2d::Size& untrimmedSize)
{
    cocos2d::Size rotatedSize = untrimmedSize;
    if (m_rotation != GAFRotation::NONE)
    {
        rotatedSize = cocos2d::Size(rotatedSize.height, rotatedSize.width);
    }
    cocos2d::Sprite::setTextureRect(rect, rotated, rotatedSize);
}

void GAFSprite::setTextureCoords(cocos2d::Rect rect)
{
    rect = CC_RECT_POINTS_TO_PIXELS(rect);

    cocos2d::Texture2D *tex = _batchNode ? _textureAtlas->getTexture() : _texture;
    if (!tex)
    {
        return;
    }

    float atlasWidth = tex->getPixelsWide();
    float atlasHeight = tex->getPixelsHigh();

	rect.origin.x /= atlasWidth;
	rect.origin.y /= atlasHeight;
	rect.size.width /= atlasWidth;
	rect.size.height /= atlasHeight;
	
	setQuadUVs(_quad, rect, m_rotation, _flippedX, _flippedY);

	if (m_scale9Enabled)
	{
		if (!m_scale9Slices.size()) m_scale9Slices.resize(9);

		Tex2F tl = _quad.tl.texCoords;
		Tex2F tr = _quad.tr.texCoords;
		Tex2F bl = _quad.bl.texCoords;
		Tex2F br = _quad.br.texCoords;

		V3F_C4B_T2F_Quad &tlq = m_scale9Slices[0];
		V3F_C4B_T2F_Quad &tcq = m_scale9Slices[1];
		V3F_C4B_T2F_Quad &trq = m_scale9Slices[2];
		V3F_C4B_T2F_Quad &clq = m_scale9Slices[3];
		V3F_C4B_T2F_Quad &ccq = m_scale9Slices[4];
		V3F_C4B_T2F_Quad &crq = m_scale9Slices[5];
		V3F_C4B_T2F_Quad &blq = m_scale9Slices[6];
		V3F_C4B_T2F_Quad &bcq = m_scale9Slices[7];
		V3F_C4B_T2F_Quad &brq = m_scale9Slices[8];

		Rect originalRect = _rect;

		float leftWidthRatio = m_capInsets.origin.x / originalRect.size.width;
		float centerWidthRatio = m_capInsets.size.width / originalRect.size.width;
		float rightWidthRatio = 1 - (leftWidthRatio + centerWidthRatio);

		float topHeightRatio = m_capInsets.origin.y / originalRect.size.height;
		float centerHeightRatio = m_capInsets.size.height / originalRect.size.height;
		float bottomHeightRatio = 1 - (topHeightRatio + centerHeightRatio);

		float uRangeW = tr.u - tl.u;
		float vRangeW = tr.v - tl.v;
		float uRangeH = bl.u - tl.u;
		float vRangeH = bl.v - tl.v;

		Tex2F tlc = { tl.u + uRangeW * leftWidthRatio, tl.v + vRangeW * leftWidthRatio };
		Tex2F trc = { tr.u - uRangeW * rightWidthRatio, tr.v - vRangeW * rightWidthRatio };
		Tex2F ltc = { tl.u + uRangeH * topHeightRatio, tl.v + vRangeH * topHeightRatio };
		Tex2F lbc = { bl.u - uRangeH * bottomHeightRatio, bl.v - vRangeH * bottomHeightRatio };

		tlq.tl.texCoords = tl;
		tlq.tr.texCoords = tcq.tl.texCoords = tlc;
		tcq.tr.texCoords = trq.tl.texCoords = trc;
		trq.tr.texCoords = tr;

		tlq.bl.texCoords = clq.tl.texCoords = ltc;
		tlq.br.texCoords = tcq.bl.texCoords = clq.tr.texCoords = ccq.tl.texCoords = { tlc.u, ltc.v };
		tcq.br.texCoords = trq.bl.texCoords = ccq.tr.texCoords = crq.tl.texCoords = { trc.u, ltc.v };
		trq.br.texCoords = crq.tr.texCoords = { tr.u, ltc.v };

		clq.bl.texCoords = blq.tl.texCoords = lbc;
		clq.br.texCoords = ccq.bl.texCoords = blq.tr.texCoords = bcq.tl.texCoords = { tlc.u, lbc.v };
		ccq.br.texCoords = crq.bl.texCoords = bcq.tr.texCoords = brq.tl.texCoords = { trc.u, lbc.v };
		crq.br.texCoords = brq.tr.texCoords = { tr.u, lbc.v };

		blq.bl.texCoords = bl;
		blq.br.texCoords = bcq.bl.texCoords = { tlc.u, bl.v };
		bcq.br.texCoords = brq.bl.texCoords = { trc.u, bl.v };
		brq.br.texCoords = br;

		tlq.tl.colors = tlq.tr.colors = tlq.bl.colors = tlq.br.colors = Color4B::WHITE;
		tcq.tl.colors = tcq.tr.colors = tcq.bl.colors = tcq.br.colors = Color4B::WHITE;
		trq.tl.colors = trq.tr.colors = trq.bl.colors = trq.br.colors = Color4B::WHITE;
		clq.tl.colors = clq.tr.colors = clq.bl.colors = clq.br.colors = Color4B::WHITE;
		ccq.tl.colors = ccq.tr.colors = ccq.bl.colors = ccq.br.colors = Color4B::WHITE;
		crq.tl.colors = crq.tr.colors = crq.bl.colors = crq.br.colors = Color4B::WHITE;
		blq.tl.colors = blq.tr.colors = blq.bl.colors = blq.br.colors = Color4B::WHITE;
		bcq.tl.colors = bcq.tr.colors = bcq.bl.colors = bcq.br.colors = Color4B::WHITE;
		brq.tl.colors = brq.tr.colors = brq.bl.colors = brq.br.colors = Color4B::WHITE;
	}
}

void GAFSprite::updateSlicedQuads(const cocos2d::Mat4 &transform)
{
	//auto currTransform = this->getNodeToParentAffineTransform();
	auto a = transform.m[0];
	auto b = transform.m[1];
	auto c = transform.m[4];
	auto d = transform.m[5];

	Size unscaledSize = this->_contentSize;

	Size topLeftSize = m_topLeftSize;
	Size bottomRightSize = m_bottomRightSize;

	float minWidth = topLeftSize.width + bottomRightSize.width;
	float minHeight = topLeftSize.height + bottomRightSize.height;

	float scaleX = sqrt(a * a + b * b);
	float scaleY = sqrt(c * c + d * d);

	float scaledWidth = unscaledSize.width * scaleX;
	float scaledHeight = unscaledSize.height * scaleY;

	// downscale corners if actional size is smaller than min size
	if (scaledWidth < minWidth)
	{
		float cornersScaleX = scaledWidth / minWidth;
		topLeftSize.width *= cornersScaleX;
		bottomRightSize.width *= cornersScaleX;
	}

	if (scaledHeight < minHeight)
	{
		float cornersScaleY = scaledHeight / minHeight;
		topLeftSize.height *= cornersScaleY;
		bottomRightSize.height *= cornersScaleY;
	}

	Vec2 rlVerticesDelta = { m_quad.tr.vertices.x - m_quad.tl.vertices.x, m_quad.tr.vertices.y - m_quad.tl.vertices.y };
	Vec2 btVerticesDelta = { m_quad.bl.vertices.x - m_quad.tl.vertices.x, m_quad.bl.vertices.y - m_quad.tl.vertices.y };

	float tlWidthRatio = topLeftSize.width / scaledWidth;
	float tlHeightRatio = topLeftSize.height / scaledHeight;
	float brWidthRatio = bottomRightSize.width / scaledWidth;
	float brHeightRatio = bottomRightSize.height / scaledHeight;

	V3F_C4B_T2F_Quad &tlq = m_scale9Slices[0];
	V3F_C4B_T2F_Quad &tcq = m_scale9Slices[1];
	V3F_C4B_T2F_Quad &trq = m_scale9Slices[2];
	V3F_C4B_T2F_Quad &clq = m_scale9Slices[3];
	V3F_C4B_T2F_Quad &ccq = m_scale9Slices[4];
	V3F_C4B_T2F_Quad &crq = m_scale9Slices[5];
	V3F_C4B_T2F_Quad &blq = m_scale9Slices[6];
	V3F_C4B_T2F_Quad &bcq = m_scale9Slices[7];
	V3F_C4B_T2F_Quad &brq = m_scale9Slices[8];

	//---------------------------------------------

	// top
	tlq.tl = m_quad.tl;

	Vec2 tempVec = rlVerticesDelta;
	tempVec.scale(tlWidthRatio);
	tlq.tr.vertices.x = tcq.tl.vertices.x = tlq.tl.vertices.x + tempVec.x;
	tlq.tr.vertices.y = tcq.tl.vertices.y = tlq.tl.vertices.y + tempVec.y;

	tempVec = rlVerticesDelta;
	tempVec.scale(brWidthRatio);
	tcq.tr.vertices.x = trq.tl.vertices.x = m_quad.tr.vertices.x - tempVec.x;
	tcq.tr.vertices.y = trq.tl.vertices.y = m_quad.tr.vertices.y - tempVec.y;

	trq.tr = m_quad.tr;

	// center-top

	tempVec = btVerticesDelta;
	tempVec.scale(tlHeightRatio);
	tlq.bl.vertices.x = clq.tl.vertices.x = tlq.tl.vertices.x + tempVec.x;
	tlq.bl.vertices.y = clq.tl.vertices.y = tlq.tl.vertices.y + tempVec.y;

	tempVec = rlVerticesDelta;
	tempVec.scale(tlWidthRatio);
	tlq.br.vertices.x = tcq.bl.vertices.x = clq.tr.vertices.x = ccq.tl.vertices.x = tlq.bl.vertices.x + tempVec.x;
	tlq.br.vertices.y = tcq.bl.vertices.y = clq.tr.vertices.y = ccq.tl.vertices.y = tlq.bl.vertices.y + tempVec.y;

	tempVec = rlVerticesDelta;
	tempVec.scale(brWidthRatio);
	tcq.br.vertices.x = trq.bl.vertices.x = ccq.tr.vertices.x = crq.tl.vertices.x = tlq.bl.vertices.x + rlVerticesDelta.x - tempVec.x;
	tcq.br.vertices.y = trq.bl.vertices.y = ccq.tr.vertices.y = crq.tl.vertices.y = tlq.bl.vertices.y + rlVerticesDelta.y - tempVec.y;

	trq.br.vertices.x = crq.tr.vertices.x = tlq.bl.vertices.x + rlVerticesDelta.x;
	trq.br.vertices.y = crq.tr.vertices.y = tlq.bl.vertices.y + rlVerticesDelta.y;

	// center-bottom

	tempVec = btVerticesDelta;
	tempVec.scale(brHeightRatio);
	clq.bl.vertices.x = blq.tl.vertices.x = tlq.tl.vertices.x + btVerticesDelta.x - tempVec.x;
	clq.bl.vertices.y = blq.tl.vertices.y = tlq.tl.vertices.y + btVerticesDelta.y - tempVec.y;

	tempVec = rlVerticesDelta;
	tempVec.scale(tlWidthRatio);
	clq.br.vertices.x = ccq.bl.vertices.x = blq.tr.vertices.x = bcq.tl.vertices.x = clq.bl.vertices.x + tempVec.x;
	clq.br.vertices.y = ccq.bl.vertices.y = blq.tr.vertices.y = bcq.tl.vertices.y = clq.bl.vertices.y + tempVec.y;

	tempVec = rlVerticesDelta;
	tempVec.scale(brWidthRatio);
	ccq.br.vertices.x = crq.bl.vertices.x = bcq.tr.vertices.x = brq.tl.vertices.x = clq.bl.vertices.x + rlVerticesDelta.x - tempVec.x;
	ccq.br.vertices.y = crq.bl.vertices.y = bcq.tr.vertices.y = brq.tl.vertices.y = clq.bl.vertices.y + rlVerticesDelta.y - tempVec.y;

	crq.br.vertices.x = brq.tr.vertices.x = clq.bl.vertices.x + rlVerticesDelta.x;
	crq.br.vertices.y = brq.tr.vertices.y = clq.bl.vertices.y + rlVerticesDelta.y;

	// bottom

	blq.bl = m_quad.bl;

	tempVec = rlVerticesDelta;
	tempVec.scale(tlWidthRatio);
	blq.br.vertices.x = bcq.bl.vertices.x = blq.bl.vertices.x + tempVec.x;
	blq.br.vertices.y = bcq.bl.vertices.y = blq.bl.vertices.y + tempVec.y;

	tempVec = rlVerticesDelta;
	tempVec.scale(brWidthRatio);
	bcq.br.vertices.x = brq.bl.vertices.x = m_quad.br.vertices.x - tempVec.x;
	bcq.br.vertices.y = brq.bl.vertices.y = m_quad.br.vertices.y - tempVec.y;

	brq.br = m_quad.br;
}

void GAFSprite::setExternalTransform(const cocos2d::AffineTransform& transform)
{
    if (!cocos2d::AffineTransformEqualToTransform(getExternalTransform(), transform))
    {
        m_externalTransform = transform;
        _transformUpdated = true;
        _transformDirty = true;
        _inverseDirty = true;
    }
}

const cocos2d::AffineTransform& GAFSprite::getExternalTransform() const
{
    return m_externalTransform;
}

const cocos2d::Mat4& GAFSprite::getNodeToParentTransform() const
{
    if (_transformDirty)
    {
        if (m_atlasScale != 1.f)
        {
            cocos2d::AffineTransform transform = cocos2d::AffineTransformScale(getExternalTransform(), m_atlasScale, m_atlasScale);
            cocos2d::CGAffineToGL(cocos2d::AffineTransformTranslate(transform, -_anchorPointInPoints.x, -_anchorPointInPoints.y), _transform.m);
        }
        else
        {
            cocos2d::CGAffineToGL(cocos2d::AffineTransformTranslate(getExternalTransform(), -_anchorPointInPoints.x, -_anchorPointInPoints.y), _transform.m);
        }
        _transformDirty = false;
    }

    return _transform;
}

cocos2d::AffineTransform GAFSprite::getNodeToParentAffineTransform() const
{
    cocos2d::AffineTransform transform;
    if (_transformDirty)
    {
        transform = getExternalTransform();
        if (m_atlasScale != 1.0f)
        {
            transform = cocos2d::AffineTransformScale(transform, m_atlasScale, m_atlasScale);
        }

        cocos2d::CGAffineToGL(cocos2d::AffineTransformTranslate(transform, -_anchorPointInPoints.x, -_anchorPointInPoints.y), _transform.m);
        _transformDirty = false;
    }
    cocos2d::GLToCGAffine(_transform.m, &transform);

    return transform;
}

#if COCOS2D_VERSION < 0x00030200
void GAFSprite::draw(cocos2d::Renderer *renderer, const cocos2d::Mat4 &transform, bool transformUpdated)
{
    (void)transformUpdated;
#else
void GAFSprite::draw(cocos2d::Renderer *renderer, const cocos2d::Mat4 &transform, uint32_t flags)
{
    (void)flags;
#endif
    if (m_isLocator)
    {
        return;
    }

    _insideBounds = (flags & FLAGS_TRANSFORM_DIRTY) ? renderer->checkVisibility(transform, _contentSize) : _insideBounds;
    if (!_insideBounds)
        return;

    uint32_t id = setUniforms();

    if (m_useSeparateBlendFunc || (m_blendEquation != -1))
    {
        m_customCommand.init(_globalZOrder);
        m_customCommand.func = CC_CALLBACK_0(GAFSprite::customDraw, this, transform);
        renderer->addCommand(&m_customCommand);
    }
    else
    {
		m_quad = _quad;

		transform.transformPoint(&m_quad.tl.vertices);
		transform.transformPoint(&m_quad.tr.vertices);
		transform.transformPoint(&m_quad.bl.vertices);
		transform.transformPoint(&m_quad.br.vertices);

		if (!m_scale9Enabled)
		{
			m_quadCommand.init(_globalZOrder, _texture->getName(), getGLProgramState(), _blendFunc, &m_quad, 1, Mat4::IDENTITY, id);
		}
		else
		{
			updateSlicedQuads(transform);
			m_quadCommand.init(_globalZOrder, _texture->getName(), getGLProgramState(), _blendFunc, &m_scale9Slices[0], 9, Mat4::IDENTITY, id);
		}

		renderer->addCommand(&m_quadCommand);
    }
}

void GAFSprite::setAtlasScale(float scale)
{
    if (m_atlasScale != scale)
    {
        m_atlasScale = scale;
        _transformDirty = true;
        _inverseDirty = true;
    }
}

uint32_t GAFSprite::setUniforms()
{
#if COCOS2D_VERSION < 0x00030300
    uint32_t materialID = QuadCommand::MATERIAL_ID_DO_NOT_BATCH;
#else
    uint32_t materialID = Renderer::MATERIAL_ID_DO_NOT_BATCH;
#endif
    if (_glProgramState->getUniformCount() == 0)
    {
        int glProgram = (int)getGLProgram()->getProgram();
        int intArray[4] = { glProgram, (int)getTexture()->getName(), (int)getBlendFunc().src, (int)getBlendFunc().dst };

        materialID = XXH32((const void*)intArray, sizeof(intArray), 0);
    }
    return materialID;
}

void GAFSprite::customDraw(cocos2d::Mat4& transform)
{
    CCAssert(!_batchNode, "If cocos2d::Sprite is being rendered by CCSpriteBatchNode, cocos2d::Sprite#draw SHOULD NOT be called");

    getGLProgramState()->apply(transform);

    if (m_useSeparateBlendFunc)
    {
        glBlendFuncSeparate(m_blendFuncSeparate.src, m_blendFuncSeparate.dst,
            m_blendFuncSeparate.srcAlpha, m_blendFuncSeparate.dstAlpha);
    }
    else
    {
        cocos2d::GL::blendFunc(_blendFunc.src, _blendFunc.dst);
    }

    if (m_blendEquation != -1)
    {
        glBlendEquation(m_blendEquation);
    }

    if (_texture != NULL)
    {
        cocos2d::GL::bindTexture2D(_texture->getName());
    }
    else
    {
        cocos2d::GL::bindTexture2D(0);
    }

    //
    // Attributes
    //

    cocos2d::GL::enableVertexAttribs(cocos2d::GL::VERTEX_ATTRIB_FLAG_POS_COLOR_TEX);
    CHECK_GL_ERROR_DEBUG();

#define kQuadSize sizeof(_quad.bl)
    long offset = (long)&_quad;

    // vertex
    int diff = offsetof(cocos2d::V3F_C4B_T2F, vertices);
    glVertexAttribPointer(cocos2d::GLProgram::VERTEX_ATTRIB_POSITION, 3, GL_FLOAT, GL_FALSE, kQuadSize, (void*)(offset + diff));

    // texCoods
    diff = offsetof(cocos2d::V3F_C4B_T2F, texCoords);
    glVertexAttribPointer(cocos2d::GLProgram::VERTEX_ATTRIB_TEX_COORDS, 2, GL_FLOAT, GL_FALSE, kQuadSize, (void*)(offset + diff));

    // color
    diff = offsetof(cocos2d::V3F_C4B_T2F, colors);
    glVertexAttribPointer(cocos2d::GLProgram::VERTEX_ATTRIB_COLOR, 4, GL_UNSIGNED_BYTE, GL_TRUE, kQuadSize, (void*)(offset + diff));

    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

    CHECK_GL_ERROR_DEBUG();

    //USING_NS_CC;
    //CC_INCREMENT_GL_DRAWN_BATCHES_AND_VERTICES(1, 4);
    //CC_INCREMENT_GL_DRAWS(1);
}

NS_GAF_END
