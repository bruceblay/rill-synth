// Copyright (c) 2026 Bruce Blay
// SPDX-License-Identifier: GPL-3.0-or-later
#include "../src/Light.h"
#include <array>
#include <cstdlib>
#include <fstream>
#include <memory>

// Six actual renderer frames, arranged into a two-row contact sheet at 2x.
int main(int argc,char** argv) {
  if(argc!=2) return 1;
  constexpr unsigned width=720,height=270;
  // The families do not arrive at the same rate: trails and growth need time
  // on the clock before there is anything to photograph, while the field
  // families are complete on their first frame. These are settled moments,
  // in twelfths of a second, not a single arbitrary count.
  static const unsigned settled[light::Painting::familyCount]={200,120,150,180,110,120};
  auto pixels=std::unique_ptr<std::array<uint16_t,width*height>>(new std::array<uint16_t,width*height>{});
  for(unsigned family=0;family<light::Painting::familyCount;++family) {
    auto painting=std::unique_ptr<light::Painting>(new light::Painting(17));
    while(painting->visualFamily()!=family) painting->regenerate();
    for(unsigned frame=0;frame<settled[family];++frame) painting->render(1.0f/12,.35f);
    for(unsigned y=0;y<135;++y) for(unsigned x=0;x<240;++x)
      (*pixels)[(y+(family/3)*135)*width+x+(family%3)*240]=painting->pixels()[y*240+x];
  }
  std::ofstream out(argv[1],std::ios::binary);
  out<<"P6\n"<<width*2<<' '<<height*2<<"\n255\n";
  for(unsigned y=0;y<height*2;++y) for(unsigned x=0;x<width*2;++x) {
    uint16_t c=(*pixels)[(y/2)*width+x/2];
    out.put(char(((c>>11)&31)*255/31));out.put(char(((c>>5)&63)*255/63));out.put(char((c&31)*255/31));
  }
  return out?0:2;
}
