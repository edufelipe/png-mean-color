What is `png-mean-color`?
=========================

This is an utility to calculate the mean color of a png file and print the
resulting color in HEX format, suitable for use on CSS and HTML.

Why?
====

I really liked the way Chrome picks the color used around thumbnails of websites
on their "Most Visited" page and wanted to do that on a program I was developing so
I found [this thread on Quora][origin] explaining the algorithm and pointing to
[the][1] [code][2] [location][3] on Chromium repository.

The original code needed `Skia` to work and had a bunch of dependencies.
I removed all dependencies except `libpng` and maintained the BSD-style license
of the original code.

How?
====

You can build it using cmake:

    $ git clone https://github.com/edufelipe/png-mean-color.git
    $ mkdir build && cd build
    $ cmake ../ && make
    $ cp png-mean-color /usr/local/bin

Then all you have to do is pass in a PNG file:

    $ png-mean-color some_file.png
    #fe6f04

[1]: http://src.chromium.org/svn/trunk/src/ui/gfx/color_analysis.h
[2]: http://src.chromium.org/svn/trunk/src/ui/gfx/color_analysis.cc
[3]: http://src.chromium.org/svn/trunk/src/ui/gfx/codec/png_codec.cc
[origin]: http://www.quora.com/Google-Chrome/How-does-Chrome-pick-the-color-for-the-stripes-on-the-Most-visited-page-thumbnails "original discussion"
