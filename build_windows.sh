x86_64-w64-mingw32-gcc -o Particles -I. src/main.c -L. -lraylib -lm -lwinmm -lgdi32 -lopengl32 -static-libgcc -static -lpthread -Ofast 
