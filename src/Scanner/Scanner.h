#include <MFRC522.h>

class Scanner : public MFRC522 {
    public:
        Scanner(
            byte ssPin,
            byte rstPin
        );
};
