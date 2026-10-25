# Norilsk licensing

The Norilsk **firmware** is free software, licensed under the GNU General Public License, version 3 only
(`GPL-3.0-only`, full text in `LICENSE`). If you distribute it, or firmware built from it, give your
recipients its complete corresponding source under the same licence.

Parts under other licences, kept with their notices:

- `firmware/src/fm6_core.c`: msfa, the six-operator FM core of Dexed (Google, Pascal Gauthier), Apache-2.0
  (`LICENSES/Apache-2.0-msfa.txt`), in the integer C port made for Felucca (Leo Kuroshita, Hügelton
  Instruments), as Tonitor shaped it, with the operators' waves added here.
- `firmware/src/dsp.h`, `firmware/src/libc.c`, `tools/gen_tables.py`: from Tonitor and Bigorna (Nuno Zimas),
  themselves after Felucca and SLOOP, GPL-3.0.
- `firmware/hal/`, the loader and the update protocol (`tools/fm1_install.py`, `docs/js/fm1ota.js`): Felucca's and
  SLOOP's (Leo Kuroshita, Hügelton Instruments), through Tonitor, GPL-3.0.
- `assets/fonts/AtkinsonHyperlegible-Bold.ttf`: the Braille Institute's Atkinson Hyperlegible, SIL Open Font
  License 1.1 (`assets/fonts/AtkinsonHyperlegible-OFL.txt`); `ter-u16b.bdf`: Terminus (`Terminus-LICENSE.txt`).

The **website** (`docs/`, `site/`, its stylesheet and `js/menu.js`, `js/demo.js`) and the **logo**
(`tools/make_logo.py`, `docs/assets/*.svg`) are under the MIT licence; the website's installer and backup scripts
(`docs/js/fm1ota.js`, `install.js`, `fm1backup.js`, `backup.js`) are GPL-3.0 with the firmware. The demos
(`docs/assets/demo/`) are rendered from the firmware's own code.
