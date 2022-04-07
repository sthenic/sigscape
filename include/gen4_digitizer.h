/*
 * A wrapper for a Gen4 digitizer in this application.
 */

#ifndef GEN4_DIGITIZER_H_H89N7D
#define GEN4_DIGITIZER_H_H89N7D

#include "digitizer.h"
#include "ADQAPI.h"

class Gen4Digitizer : public Digitizer
{
public:
    Gen4Digitizer(void *control_unit, int index);
    Gen4Digitizer(const Gen4Digitizer &other) = delete;
    Gen4Digitizer &operator=(const Gen4Digitizer &other) = delete;

private:
    const struct Identifier
    {
        Identifier(void *control_unit, int index)
            : control_unit(control_unit)
            , index(index)
        {}

        void *control_unit;
        int index;
    } m_id;

    void MainLoop() override;
    void ProcessMessages();
};

#endif
