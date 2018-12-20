/*
 * Copyright 2018 Metrological
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "ParsePSSHHeader.h"

namespace {

constexpr uint8_t CommonEncryption[] = { 0x10, 0x77, 0xef, 0xec, 0xc0, 0xb2, 0x4d, 0x02, 0xac, 0xe3, 0x3c, 0x1e, 0x52, 0xe2, 0xfb, 0x4b };

}

int32_t CDMi::FindPSSHHeaderPrivateData(const uint8_t*& data, const uint32_t length) {

    WPEFramework::Core::FrameType<0> frame(const_cast<uint8_t *>(data), length, length);
    WPEFramework::Core::FrameType<0>::Reader reader(frame, 0);

    // parse pssh header
    int32_t result = -1; //not enough data
    uint32_t remainingsize = reader.Number<uint32_t>();
    ASSERT(remainingsize <= length);
    ASSERT(remainingsize >= 4); 
    remainingsize -= 4;

    if( remainingsize >= 4 ) {
        uint32_t psshident = reader.Number<uint32_t>();
        result = psshident == 0x70737368 ? result : -2;   
        remainingsize -= 4;     

        if ( result == -1 && remainingsize >= 4 ) {
            uint32_t header = reader.Number<uint32_t>();
            remainingsize -= 4;     
            constexpr uint16_t buffersize = 16;

            if ( remainingsize >= buffersize ) {
                uint8_t buffer[buffersize];
                reader.Copy(buffersize, buffer);
                result = ( memcmp (buffer, CommonEncryption, buffersize)  == 0 ) ? result : -3;
                remainingsize -= buffersize;     

                if ( result == -1 && remainingsize >= 4 ) {

                    const uint8_t* buffer = nullptr;
                    uint32_t KIDcount = reader.LockBuffer<uint32_t>(buffer);
                    remainingsize -= 4;
                    
                    //for now we are not interested in the KIDs
                    if( remainingsize >= ( KIDcount * 16 ) ) {
                        reader.UnlockBuffer(KIDcount * 16);    
                        remainingsize -= ( KIDcount * buffersize );

                        //now we got to the private data we are looking for...
                        if( remainingsize >= 4 ) {
                            uint32_t datalength = reader.Number<uint32_t>();
                            remainingsize -= 4;
                            ASSERT(datalength == remainingsize);
                            result = datalength;
                            data = &data[length-reader.Length()]; // pointer will be invalid of course when datalength is 0 but then should be ignored by callee
                        }
                    }
                }
            }
        }
    }

    return result;
}
