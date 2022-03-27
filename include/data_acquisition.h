/*
 * An abstract base class that defines the interface of a generic data
 * acquisition process.
 */

#ifndef DATA_ACQUISITION_H_FONFCY
#define DATA_ACQUISITION_H_FONFCY

class DataAcquisition
{
public:
    virtual int Start() = 0;
    virtual int Stop() = 0;
    virtual int WaitForBuffer(std::shared_ptr<void> &buffer, int timeout, void *status) = 0;
    virtual int ReturnBuffer(std::shared_ptr<void> buffer) = 0;
};

#endif
