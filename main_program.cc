// Copyright (c) 2011 Eduardo Felipe Castegnaro. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "color_analysis.h"

#include <algorithm>
#include <cstdio>
#include <fstream>
#include <iostream>
#include <iterator>

int main (int argc, char const *argv[])
{
    if (argc < 2) {
        std::cerr << "USAGE:\n png-mean-color INPUT.png" << std::endl;
        return 1;
    }

    std::ifstream pngfile;
    pngfile.open(argv[1]);
    if (pngfile.fail()) {
        std::cerr << "Invalid file." << std::endl;
        return 1;
    }

    std::vector<uint8_t> data;
    std::copy(std::istreambuf_iterator<char>(pngfile.rdbuf()),
        std::istreambuf_iterator<char>(), std::back_inserter(data));

    color_utils::ARGBColor color = color_utils::CalculateRecommendedBgColorForPNG(data);

    char result[9] = {0};
    std::sprintf(result, "%02x%02x%02x", (color >> 16) & 0xFF,
        (color >>  8) & 0xFF, (color >> 0) & 0xFF);

    std::cout << "#" << result << std::endl;
    return 0;
}
