#!/bin/bash
convert -font /usr/share/fonts/TTF/verdana.ttf -depth 8 -trim keys.txt Help.ppm 
cat Help.ppm | sed 1,3d > a && mv a Help.raw 
identify Help.ppm | awk '{print $3}' | awk -Fx '{printf("#define HELPW %s\n", $1);}' > ../src/HelpKeys.h
identify Help.ppm | awk '{print $3}' | awk -Fx '{printf("#define HELPH %s\n", $2);}' >> ../src/HelpKeys.h
identify Help.ppm | sed 's,^,//,' >> ../src/HelpKeys.h 
bin2header helpKeysImage < Help.raw | sed 1d >> ../src/HelpKeys.h 
rm Help.ppm Help.raw
convert -font /usr/share/fonts/TTF/verdana.ttf -depth 8 -trim onScreenKeys.txt OnlineHelp.ppm 
cat OnlineHelp.ppm | sed 1,3d > a && mv a OnlineHelp.raw 
cat > ../src/OnlineHelpKeys.h <<EF
#ifndef __ONLINEHELPKEYS_H__
#define __ONLINEHELPKEYS_H__

EF
identify OnlineHelp.ppm | awk '{print $3}' | awk -Fx '{printf("#define OHELPW %s\n", $1);}' >> ../src/OnlineHelpKeys.h
identify OnlineHelp.ppm | awk '{print $3}' | awk -Fx '{printf("#define OHELPH %s\n", $2);}' >> ../src/OnlineHelpKeys.h
cat >> ../src/OnlineHelpKeys.h <<OEF

#ifndef DEFINE_VARS
extern unsigned char onlineHelpKeysImage[];
#else
OEF
identify OnlineHelp.ppm | sed 's,^,//,' >> ../src/OnlineHelpKeys.h 
bin2header onlineHelpKeysImage < OnlineHelp.raw | sed 1d >> ../src/OnlineHelpKeys.h 
cat >> ../src/OnlineHelpKeys.h <<OEF2
#endif

#endif
OEF2
rm OnlineHelp.ppm OnlineHelp.raw
