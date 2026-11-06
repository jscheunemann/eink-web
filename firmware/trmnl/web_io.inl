//
// eink-web: bb_epaper I/O backend that emulates the panel controller.
//
// On the device, bb_epaper sends commands and image data to the panel over
// SPI. Here those bytes land in an emulated controller: image-RAM writes fill
// two bitplanes (honouring partial windows), and a refresh command hands the
// planes to the page, which draws them on a canvas. Text screens drawn into
// bb_epaper's buffer and images the firmware streams straight to the panel
// both pass through this one path.
//
// Included by bb_epaper.cpp in place of its Arduino I/O file (see
// firmware/trmnl/patch_trmnl.py). UC81xx controllers (TRMNL OG panels) are
// emulated; SSD16xx commands are recognised for writes and refresh.
//
#ifndef __BB_EP_IO__
#define __BB_EP_IO__

#include <stdint.h>
#include <string.h>

#include <vector>

#include "eink_web.h"

void bbepWakeUp(BBEPDISP *pBBEP);
void bbepSendCMDSequence(BBEPDISP *pBBEP, const uint8_t *pSeq);

namespace eink_web_panel {

constexpr uint8_t UC_DTM1 = 0x10, UC_DRF = 0x12, UC_DTM2 = 0x13, UC_PTL = 0x90, UC_PTIN = 0x91, UC_PTOUT = 0x92;
constexpr uint8_t SSD_ACTIVATE = 0x20, SSD_RAM = 0x24, SSD_ALTRAM = 0x26;

struct Controller {
  int width = 0, height = 0;   // native panel size in pixels
  std::vector<uint8_t> plane[2];  // [0] = "new" RAM (DTM2 / WRITE_RAM), [1] = "old" RAM (DTM1 / ALTRAM)
  uint8_t cmd = 0;
  int target = -1;             // plane receiving data, or -1
  bool partial = false;
  int wx0 = 0, wx1 = 0, wy0 = 0, wy1 = 0;  // window in pixels, inclusive
  uint8_t ptl[9];
  int ptlLen = 0;
  long pos = 0;                // bytes written into the window since the write command

  void ensure(BBEPDISP *p) {
    if (width == p->native_width && height == p->native_height && !plane[0].empty()) return;
    width = p->native_width;
    height = p->native_height;
    for (auto &pl : plane) pl.assign((size_t)((width + 7) / 8) * height, 0xff);
    fullWindow();
  }
  void fullWindow() {
    wx0 = 0;
    wx1 = width - 1;
    wy0 = 0;
    wy1 = height - 1;
  }
  void command(BBEPDISP *p, uint8_t c) {
    ensure(p);
    cmd = c;
    target = -1;
    // The two controller families reuse command numbers: on UC81xx, 0x20-0x24
    // load waveform tables; on SSD16xx they activate and write RAM.
    if (p->chip_type == BBEP_CHIP_SSD16xx) {
      switch (c) {
        case SSD_RAM: target = 0; pos = 0; break;
        case SSD_ALTRAM: target = 1; pos = 0; break;
        case SSD_ACTIVATE: present(p); break;
        default: break;
      }
      return;
    }
    switch (c) {
      case UC_DTM2: target = 0; pos = 0; break;
      case UC_DTM1: target = 1; pos = 0; break;
      case UC_PTIN: partial = true; break;
      case UC_PTOUT: partial = false; fullWindow(); break;
      case UC_PTL: ptlLen = 0; break;
      case UC_DRF: present(p); break;
      default: break;
    }
  }
  void data(BBEPDISP *p, const uint8_t *d, int n) {
    ensure(p);
    if (cmd == UC_PTL) {
      for (int i = 0; i < n && ptlLen < 9; i++) ptl[ptlLen++] = d[i];
      if (ptlLen == 9) {  // 2-byte coordinates: x start/end, y start/end, scan flag
        wx0 = (ptl[0] << 8) | ptl[1];
        wx1 = (ptl[2] << 8) | ptl[3];
        wy0 = (ptl[4] << 8) | ptl[5];
        wy1 = (ptl[6] << 8) | ptl[7];
      }
      return;
    }
    if (target < 0) return;
    const int x0 = partial ? wx0 : 0, x1 = partial ? wx1 : width - 1;
    const int y0 = partial ? wy0 : 0, y1 = partial ? wy1 : height - 1;
    const int rowBytes = (x1 - x0 + 1 + 7) / 8;
    const int pitch = (width + 7) / 8;
    std::vector<uint8_t> &pl = plane[target];
    for (int i = 0; i < n; i++, pos++) {
      const long row = pos / rowBytes, col = pos % rowBytes;
      if (y0 + row > y1) break;
      const size_t off = (size_t)(y0 + row) * pitch + (x0 / 8) + col;
      if (off < pl.size()) pl[off] = d[i];
    }
  }
  // Hand both planes to the page with the panel's colour mode.
  void present(BBEPDISP *p) {
    ensure(p);
    const int gray4 = (p->iFlags & BBEP_4GRAY) ? 1 : 0;
    eink_web_present(plane[0].data(), plane[1].data(), width, height, gray4);
  }
};

inline Controller &controller() {
  static Controller c;
  return c;
}

}  // namespace eink_web_panel

void bbepInitIO(BBEPDISP *pBBEP, uint8_t u8DC, uint8_t u8RST, uint8_t u8BUSY, uint8_t u8CS, uint8_t u8MOSI,
                uint8_t u8SCK, uint32_t u32Speed) {
  pBBEP->iDCPin = u8DC;
  pBBEP->iCSPin = u8CS;
  pBBEP->iMOSIPin = u8MOSI;
  pBBEP->iCLKPin = u8SCK;
  pBBEP->iRSTPin = u8RST;
  pBBEP->iBUSYPin = u8BUSY;
  (void)u32Speed;
}

void bbepSetCS2(BBEPDISP *pBBEP, uint8_t cs) { pBBEP->iCS2Pin = cs; }

void bbepWriteCmd(BBEPDISP *pBBEP, uint8_t cmd) {
  pBBEP->is_awake = 1;
  eink_web_panel::controller().command(pBBEP, cmd);
}

void bbepWriteData(BBEPDISP *pBBEP, uint8_t *pData, int iLen) {
  eink_web_panel::controller().data(pBBEP, pData, iLen);
}

void bbepWriteCmdData(BBEPDISP *pBBEP, uint8_t cmd, uint8_t *pData, int iLen) {
  bbepWriteCmd(pBBEP, cmd);
  if (iLen > 0) bbepWriteData(pBBEP, pData, iLen);
}

void bbepCMD2(BBEPDISP *pBBEP, uint8_t cmd1, uint8_t cmd2) {
  bbepWriteCmd(pBBEP, cmd1);
  bbepWriteData(pBBEP, &cmd2, 1);
}

// IT8951 (parallel controller) is not emulated; these keep the linker happy.
void bbepWriteIT8951Cmd(BBEPDISP *, uint16_t) {}
void bbepWriteIT8951Data(BBEPDISP *, uint8_t *, int) {}
void bbepWriteIT8951CmdArgs(BBEPDISP *, uint16_t, uint16_t *, int) {}

#endif  // __BB_EP_IO__
