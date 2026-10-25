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
