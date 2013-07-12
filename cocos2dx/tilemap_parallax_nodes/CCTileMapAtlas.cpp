/****************************************************************************
Copyright (c) 2010-2012 cocos2d-x.org
Copyright (c) 2008-2010 Ricardo Quesada
Copyright (c) 2011      Zynga Inc.

http://www.cocos2d-x.org

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
THE SOFTWARE.
****************************************************************************/
#include "CCTileMapAtlas.h"
#include "platform/CCFileUtils.h"
#include "textures/CCTextureAtlas.h"
#include "support/image_support/TGAlib.h"
#include "ccConfig.h"
#include "cocoa/CCDictionary.h"
#include "cocoa/CCInteger.h"
#include "CCDirector.h"
#include "support/CCPointExtension.h"

NS_CC_BEGIN

// implementation TileMapAtlas

TileMapAtlas * TileMapAtlas::create(const char *tile, const char *mapFile, int tileWidth, int tileHeight)
{
    TileMapAtlas *pRet = new TileMapAtlas();
    if (pRet->initWithTileFile(tile, mapFile, tileWidth, tileHeight))
    {
        pRet->autorelease();
        return pRet;
    }
    CC_SAFE_DELETE(pRet);
    return NULL;
}

bool TileMapAtlas::initWithTileFile(const char *tile, const char *mapFile, int tileWidth, int tileHeight)
{
    this->loadTGAfile(mapFile);
    this->calculateItemsToRender();

    if( AtlasNode::initWithTileFile(tile, tileWidth, tileHeight, _itemsToRender) )
    {
        _posToAtlasIndex = new Dictionary();
        this->updateAtlasValues();
        this->setContentSize(Size((float)(_TGAInfo->width*_itemWidth),
                                        (float)(_TGAInfo->height*_itemHeight)));
        return true;
    }
    return false;
}

TileMapAtlas::TileMapAtlas()
    :_TGAInfo(NULL)
    ,_posToAtlasIndex(NULL)
    ,_itemsToRender(0)
{
}

TileMapAtlas::~TileMapAtlas()
{
    if (_TGAInfo)
    {
        tgaDestroy(_TGAInfo);
    }
    CC_SAFE_RELEASE(_posToAtlasIndex);
}

void TileMapAtlas::releaseMap()
{
    if (_TGAInfo)
    {
        tgaDestroy(_TGAInfo);
    }
    _TGAInfo = NULL;

    CC_SAFE_RELEASE_NULL(_posToAtlasIndex);
}

void TileMapAtlas::calculateItemsToRender()
{
    CCAssert( _TGAInfo != NULL, "tgaInfo must be non-nil");

    _itemsToRender = 0;
    for(int x=0;x < _TGAInfo->width; x++ ) 
    {
        for( int y=0; y < _TGAInfo->height; y++ ) 
        {
            Color3B *ptr = (Color3B*) _TGAInfo->imageData;
            Color3B value = ptr[x + y * _TGAInfo->width];
            if( value.r )
            {
                ++_itemsToRender;
            }
        }
    }
}

void TileMapAtlas::loadTGAfile(const char *file)
{
    CCAssert( file != NULL, "file must be non-nil");

    std::string fullPath = FileUtils::sharedFileUtils()->fullPathForFilename(file);

    //    //Find the path of the file
    //    NSBundle *mainBndl = [Director sharedDirector].loadingBundle;
    //    String *resourcePath = [mainBndl resourcePath];
    //    String * path = [resourcePath stringByAppendingPathComponent:file];

    _TGAInfo = tgaLoad( fullPath.c_str() );
#if 1
    if( _TGAInfo->status != TGA_OK ) 
    {
        CCAssert(0, "TileMapAtlasLoadTGA : TileMapAtlas cannot load TGA file");
    }
#endif
}

// TileMapAtlas - Atlas generation / updates
void TileMapAtlas::setTile(const Color3B& tile, const Point& position)
{
    CCAssert(_TGAInfo != NULL, "tgaInfo must not be nil");
    CCAssert(_posToAtlasIndex != NULL, "posToAtlasIndex must not be nil");
    CCAssert(position.x < _TGAInfo->width, "Invalid position.x");
    CCAssert(position.y < _TGAInfo->height, "Invalid position.x");
    CCAssert(tile.r != 0, "R component must be non 0");

    Color3B *ptr = (Color3B*)_TGAInfo->imageData;
    Color3B value = ptr[(unsigned int)(position.x + position.y * _TGAInfo->width)];
    if( value.r == 0 )
    {
        CCLOG("cocos2d: Value.r must be non 0.");
    } 
    else
    {
        ptr[(unsigned int)(position.x + position.y * _TGAInfo->width)] = tile;

        // XXX: this method consumes a lot of memory
        // XXX: a tree of something like that shall be implemented
        Integer *num = (Integer*)_posToAtlasIndex->objectForKey(String::createWithFormat("%ld,%ld", 
                                                                                                 (long)position.x, 
                                                                                                 (long)position.y)->getCString());
        this->updateAtlasValueAt(position, tile, num->getValue());
    }    
}

Color3B TileMapAtlas::tileAt(const Point& position)
{
    CCAssert( _TGAInfo != NULL, "tgaInfo must not be nil");
    CCAssert( position.x < _TGAInfo->width, "Invalid position.x");
    CCAssert( position.y < _TGAInfo->height, "Invalid position.y");

    Color3B *ptr = (Color3B*)_TGAInfo->imageData;
    Color3B value = ptr[(unsigned int)(position.x + position.y * _TGAInfo->width)];

    return value;    
}

void TileMapAtlas::updateAtlasValueAt(const Point& pos, const Color3B& value, unsigned int index)
{
    CCAssert( index >= 0 && index < _textureAtlas->getCapacity(), "updateAtlasValueAt: Invalid index");

    V3F_C4B_T2F_Quad* quad = &((_textureAtlas->getQuads())[index]);

    int x = pos.x;
    int y = pos.y;
    float row = (float) (value.r % _itemsPerRow);
    float col = (float) (value.r / _itemsPerRow);

    float textureWide = (float) (_textureAtlas->getTexture()->getPixelsWide());
    float textureHigh = (float) (_textureAtlas->getTexture()->getPixelsHigh());

    float itemWidthInPixels = _itemWidth * CC_CONTENT_SCALE_FACTOR();
    float itemHeightInPixels = _itemHeight * CC_CONTENT_SCALE_FACTOR();

#if CC_FIX_ARTIFACTS_BY_STRECHING_TEXEL
    float left        = (2 * row * itemWidthInPixels + 1) / (2 * textureWide);
    float right       = left + (itemWidthInPixels * 2 - 2) / (2 * textureWide);
    float top         = (2 * col * itemHeightInPixels + 1) / (2 * textureHigh);
    float bottom      = top + (itemHeightInPixels * 2 - 2) / (2 * textureHigh);
#else
    float left        = (row * itemWidthInPixels) / textureWide;
    float right       = left + itemWidthInPixels / textureWide;
    float top         = (col * itemHeightInPixels) / textureHigh;
    float bottom      = top + itemHeightInPixels / textureHigh;
#endif

    quad->tl.texCoords.u = left;
    quad->tl.texCoords.v = top;
    quad->tr.texCoords.u = right;
    quad->tr.texCoords.v = top;
    quad->bl.texCoords.u = left;
    quad->bl.texCoords.v = bottom;
    quad->br.texCoords.u = right;
    quad->br.texCoords.v = bottom;

    quad->bl.vertices.x = (float) (x * _itemWidth);
    quad->bl.vertices.y = (float) (y * _itemHeight);
    quad->bl.vertices.z = 0.0f;
    quad->br.vertices.x = (float)(x * _itemWidth + _itemWidth);
    quad->br.vertices.y = (float)(y * _itemHeight);
    quad->br.vertices.z = 0.0f;
    quad->tl.vertices.x = (float)(x * _itemWidth);
    quad->tl.vertices.y = (float)(y * _itemHeight + _itemHeight);
    quad->tl.vertices.z = 0.0f;
    quad->tr.vertices.x = (float)(x * _itemWidth + _itemWidth);
    quad->tr.vertices.y = (float)(y * _itemHeight + _itemHeight);
    quad->tr.vertices.z = 0.0f;

    Color4B color(_displayedColor.r, _displayedColor.g, _displayedColor.b, _displayedOpacity);
    quad->tr.colors = color;
    quad->tl.colors = color;
    quad->br.colors = color;
    quad->bl.colors = color;

    _textureAtlas->setDirty(true);
    unsigned int totalQuads = _textureAtlas->getTotalQuads();
    if (index + 1 > totalQuads) {
        _textureAtlas->increaseTotalQuadsWith(index + 1 - totalQuads);
    }
}

void TileMapAtlas::updateAtlasValues()
{
    CCAssert( _TGAInfo != NULL, "tgaInfo must be non-nil");

    int total = 0;

    for(int x=0;x < _TGAInfo->width; x++ ) 
    {
        for( int y=0; y < _TGAInfo->height; y++ ) 
        {
            if( total < _itemsToRender ) 
            {
                Color3B *ptr = (Color3B*) _TGAInfo->imageData;
                Color3B value = ptr[x + y * _TGAInfo->width];

                if( value.r != 0 )
                {
                    this->updateAtlasValueAt(Point(x,y), value, total);

                    String *key = String::createWithFormat("%d,%d", x,y);
                    Integer *num = Integer::create(total);
                    _posToAtlasIndex->setObject(num, key->getCString());

                    total++;
                }
            }
        }
    }
}

void TileMapAtlas::setTGAInfo(struct sImageTGA* var)
{
    _TGAInfo = var;
}

struct sImageTGA * TileMapAtlas::getTGAInfo()
{
    return _TGAInfo;
}

NS_CC_END
