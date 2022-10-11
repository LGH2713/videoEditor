#include "frame.h"

Frame::Frame()
{

}

Frame::~Frame()
{

}

void Frame::frame_queue_unref_item(Frame *vp)
{
    av_frame_unref(vp->frame);
}
