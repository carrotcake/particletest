#version 430

layout (local_size_x = 1, local_size_y = 1, local_size_z = 1) in;

struct Box{
    vec2 pos;
    vec2 velocity;
    uint color;
    float size;
};

layout (std430, binding = 1) buffer boxBufferLayout
{
  Box boxBuffer[];
};


void main(void)
{
    uint idx = gl_GlobalInvocationID.x;

    if(boxBuffer[idx].size > 0)
    {
        for(uint i = 0; i < gl_NumWorkGroups; i++){
            if(boxBuffer[i].size <= 0) {
                continue;
            }
            const float radius = ;


        }

    }

}
