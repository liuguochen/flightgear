// A text on the canvas
//
// Copyright (C) 2012  Thomas Geymayer <tomgey@gmail.com>
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License as
// published by the Free Software Foundation; either version 2 of the
// License, or (at your option) any later version.
//
// This program is distributed in the hope that it will be useful, but
// WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
// General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.

#include "text.hxx"
#include <Canvas/property_helper.hxx>

#include <Main/globals.hxx>
#include <Main/fg_props.hxx>

#include <osgText/Text>

namespace canvas
{
  class Text::TextOSG:
    public osgText::Text
  {
    public:

      void setCharacterAspect(float aspect);
      void setFill(const std::string& fill);
      void setBackgroundColor(const std::string& fill);

      osg::Vec2 handleHit(float x, float y);

      virtual osg::BoundingBox computeBound() const;
  };

  //----------------------------------------------------------------------------
  void Text::TextOSG::setCharacterAspect(float aspect)
  {
    setCharacterSize(getCharacterHeight(), aspect);
  }

  //----------------------------------------------------------------------------
  void Text::TextOSG::setFill(const std::string& fill)
  {
//    if( fill == "none" )
//      TODO No text
//    else
      setColor( parseColor(fill) );
  }

  //----------------------------------------------------------------------------
  void Text::TextOSG::setBackgroundColor(const std::string& fill)
  {
    setBoundingBoxColor( parseColor(fill) );
  }

  //----------------------------------------------------------------------------
  osg::Vec2 Text::TextOSG::handleHit(float x, float y)
  {
    float line_height = _characterHeight + _lineSpacing;

    // TODO check with align other than TOP
    float first_line_y = -0.5 * _lineSpacing;//_offset.y() - _characterHeight;
    size_t line = std::max<int>(0, (y - first_line_y) / line_height);

    if( _textureGlyphQuadMap.empty() )
      return osg::Vec2(-1, -1);

    // TODO check when it can be larger
    assert( _textureGlyphQuadMap.size() == 1 );

    const GlyphQuads& glyphquad = _textureGlyphQuadMap.begin()->second;
    const GlyphQuads::Glyphs& glyphs = glyphquad._glyphs;
    const GlyphQuads::Coords2& coords = glyphquad._coords;
    const GlyphQuads::LineNumbers& line_numbers = glyphquad._lineNumbers;

    const float HIT_FRACTION = 0.6;
    const float character_width = getCharacterHeight()
                                * getCharacterAspectRatio();

    y = (line + 0.5) * line_height;

    bool line_found = false;
    for(size_t i = 0; i < line_numbers.size(); ++i)
    {
      if( line_numbers[i] != line )
      {
        if( !line_found )
        {
          if( line_numbers[i] < line )
            // Wait for the correct line...
            continue;

          // We have already passed the correct line -> It's empty...
          return osg::Vec2(0, y);
        }

        // Next line and not returned -> not before any character
        // -> return position after last character of line
        return osg::Vec2(coords[(i - 1) * 4 + 2].x(), y);
      }

      line_found = true;

      // Get threshold for mouse x position for setting cursor before or after
      // current character
      float threshold = coords[i * 4].x()
                      + HIT_FRACTION * glyphs[i]->getHorizontalAdvance()
                                     * character_width;

      if( x <= threshold )
      {
        if( i == 0 || line_numbers[i - 1] != line )
          // first character of line
          x = coords[i * 4].x();
        else if( coords[(i - 1) * 4].x() == coords[(i - 1) * 4 + 2].x() )
          // If previous character width is zero set to begin of next character
          // (Happens eg. with spaces)
          x = coords[i * 4].x();
        else
          // position at center between characters
          x = 0.5 * (coords[(i - 1) * 4 + 2].x() + coords[i * 4].x());

        return osg::Vec2(x, y);
      }
    }

    // Nothing found -> return position after last character
    return osg::Vec2
    (
      coords.back().x(),
      (_lineCount - 0.5) * line_height
    );
  }

  //----------------------------------------------------------------------------
  osg::BoundingBox Text::TextOSG::computeBound() const
  {
    osg::BoundingBox bb = osgText::Text::computeBound();
    if( !bb.valid() )
      return bb;

    // TODO bounding box still doesn't seem always right (eg. with center
    //      horizontal alignment not completely accurate)
    bb._min.y() += _offset.y();
    bb._max.y() += _offset.y();

    return bb;
  }

  //----------------------------------------------------------------------------
  Text::Text(SGPropertyNode_ptr node, const Style& parent_style):
    Element(node, parent_style, BOUNDING_BOX),
    _text( new Text::TextOSG() )
  {
    setDrawable(_text);
    _text->setCharacterSizeMode(osgText::Text::OBJECT_COORDS);
    _text->setAxisAlignment(osgText::Text::USER_DEFINED_ROTATION);
    _text->setRotation(osg::Quat(osg::PI, osg::X_AXIS));

    addStyle("fill", &TextOSG::setFill, _text);
    addStyle("background", &TextOSG::setBackgroundColor, _text);
    addStyle("character-size",
             static_cast<void (TextOSG::*)(float)>(&TextOSG::setCharacterSize),
             _text);
    addStyle("character-aspect-ratio", &TextOSG::setCharacterAspect, _text);
    addStyle("padding", &TextOSG::setBoundingBoxMargin, _text);
    //  TEXT              = 1 default
    //  BOUNDINGBOX       = 2
    //  FILLEDBOUNDINGBOX = 4
    //  ALIGNMENT         = 8
    addStyle<int>("draw-mode", &TextOSG::setDrawMode, _text);
    addStyle("max-width", &TextOSG::setMaximumWidth, _text);
    addStyle("font", &Text::setFont, this);
    addStyle("alignment", &Text::setAlignment, this);
    addStyle("text", &Text::setText, this);

    setupStyle();
  }

  //----------------------------------------------------------------------------
  Text::~Text()
  {

  }

  //----------------------------------------------------------------------------
  void Text::setText(const char* text)
  {
    _text->setText(text, osgText::String::ENCODING_UTF8);
  }

  //----------------------------------------------------------------------------
  void Text::setFont(const char* name)
  {
    _text->setFont( getFont(name) );
  }

  //----------------------------------------------------------------------------
  void Text::setAlignment(const char* align)
  {
    const std::string align_string(align);
    if( 0 ) return;
#define ENUM_MAPPING(enum_val, string_val) \
    else if( align_string == string_val )\
      _text->setAlignment( osgText::Text::enum_val );
#include "text-alignment.hxx"
#undef ENUM_MAPPING
    else
    {
      if( !align_string.empty() )
        SG_LOG
        (
          SG_GENERAL,
          SG_WARN,
          "canvas::Text: unknown alignment '" << align_string << "'"
        );
      _text->setAlignment(osgText::Text::LEFT_BASE_LINE);
    }
  }

  //----------------------------------------------------------------------------
#if 0
  const char* Text::getAlignment() const
  {
    switch( _text->getAlignment() )
    {
#define ENUM_MAPPING(enum_val, string_val) \
      case osgText::Text::enum_val:\
        return string_val;
#include "text-alignment.hxx"
#undef ENUM_MAPPING
      default:
        return "unknown";
    }
  }
#endif

  //----------------------------------------------------------------------------
  void Text::childChanged(SGPropertyNode* child)
  {
    if( child->getParent() != _node )
      return;

    const std::string& name = child->getNameString();
    if( name == "hit-y" )
      handleHit
      (
        _node->getFloatValue("hit-x"),
        _node->getFloatValue("hit-y")
      );
  }

  //----------------------------------------------------------------------------
  void Text::handleHit(float x, float y)
  {
    const osg::Vec2& pos = _text->handleHit(x, y);
    _node->setFloatValue("cursor-x", pos.x());
    _node->setFloatValue("cursor-y", pos.y());
  }

  //----------------------------------------------------------------------------
  Text::font_ptr Text::getFont(const std::string& name)
  {
    SGPath path = globals->resolve_ressource_path("Fonts/" + name);
    if( path.isNull() )
    {
      SG_LOG
      (
        SG_GL,
        SG_ALERT,
        "canvas::Text: No such font: " << name
      );
      return font_ptr();
    }

    SG_LOG
    (
      SG_GL,
      SG_INFO,
      "canvas::Text: using font file " << path.str()
    );

    font_ptr font = osgText::readFontFile(path.c_str());
    if( !font )
      SG_LOG
      (
        SG_GL,
        SG_ALERT,
        "canvas::Text: Failed to open font file " << path.c_str()
      );

    return font;
  }

} // namespace canvas