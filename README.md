# Svg2ass

Convert SVG vector graphics to ASS subtitle draw instructions.


## Overview

SVG2ASS feeds on SVG files and drops ASS dialog lines, ready for
pasting in e.g. Aegisub. Only a basic set of SVG funtionality is
supported, whereas most of the more advanced features are simply
ignored. You can work around some of these limitations by converting
all objects to paths and flattening your SVG as much as feasible.
The SVG/XML is not validated at all - garbage in, garbage out!

If you don't intend to build svg2ass yourself or just want to give
it a quick test run, you may want to check out Gustavo Rodrigues'
[svg2ass-gui](https://qgustavor.github.io/svg2ass-gui/).


### Supported SVG elements

  * g(roup)
  * line
  * rect
  * circle, ellipse
  * polyline, polygon
  * path

### Supported SVG attributes

  * attributes essential to the elements listed above
  * select presentation attributes and inline CSS style attributes
    (colors/alpha for fill and stroke; stroke width)
  * transform (translate, scale, rotate, skewX, skewY, matrix)

### Output format

The ASS dialog lines are generated according to this pattern:
```
    Dialogue: Layer,Start,End,Style,Name,MarginL,MarginR,MarginV,Effect,Text
```

**Caveat:**

  * Resulting shapes will appear noticeable larger than expected for
    stroke widths >1, due to the different semantics of SVG strokes
    and ASS borders. Deal with it, as this is not trivial to fix!
  * Keep in mind that ASS does not support "open" paths, only closed
    shapes. Thus any open ended path in SVG will be forcefully closed
    by any ASS interpreter after conversion. It is advisable to avoid
    open paths altogether in SVGs subject to ASS conversion. This
    applies to the polyline SVG element as well.
  * There is no explicit Unicode support build into the application.
    However, since only SVG keywords and attribute values are parsed,
    UTF-8 input should be processed just fine.
  * Thanks to the oversimplified parser there is no guarantee svg2ass
    accepts and sensibly converts any old SVG document.


## Build

SVG2ASS should easily build on an average GNU/Linux or other POSIX
compliant system by just executing make. On other systems just slab
it together using compiler and linker, see Makefile - nothing fancy
there. However, depending on your runtime environment, you might
have provide your own getopt, strdup or strcasecmp implementations.
Also, you might have to manually copy version.in to version.h

Apparently, it is advisable to change `strip -s` to `strip -S` in
Makefile when building on macOS.

In case you wish to avoid the hassle of building from source altogether:
As mentioned above, Gustavo Rodrigues created
[svg2ass-gui](https://github.com/qgustavor/svg2ass-gui), a web GUI based
on the svg2ass source code. You can try it live
[here](https://qgustavor.github.io/svg2ass-gui/).


## Usage

Executing svg2ass -h displays a short help text. Among other general
and ASS related, fairly self explaining command line options, SVG2ASS
provides an option to render all shapes into a single drawing command
per SVG file (which forces all shapes to use the same initial color
and border width settings), or alternatively, produce a separate
dialog line for each shape (which is the default).


## License

Svg2ass is distributed under the Modified ("3-clause") BSD License.
See `LICENSE` for more information.

----------------------------------------------------------------------
