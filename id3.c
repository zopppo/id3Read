#include "id3.h"
#include <stdarg.h>
#include <wchar.h>

// Map the song to an array
unsigned char *file;
header_t *getHeader();
frame_t *getNextFrame();
// Inreases the position pointer by n
void increasePos(int n);
int currentPosition = 0;

void printHeader(header_t *header);
void printFrame(frame_t *frame);
void errExit(char *errMsg, ...);

//char *frameIDs[40] = {"

int main(int argc, char *argv[])
{
    // Verify Arguments
    if (argc < 2) {
        printf("Usage: id3Reader <mp3 songFilename>\n");
        exit(1);
    }

    char *songFilename = argv[1];

    int fd = open(songFilename, O_RDONLY);
    struct stat st;
    fstat(fd, &st); 
    //
    // Couldn't open the file
    if (fd == -1) {
        if (errno == EACCES) {
            errExit("Do not have access to open %s.", songFilename);
        }
    }

    file = mmap(NULL, st.st_size, PROT_READ, MAP_SHARED, fd, 0);
    // Gets the header_t from the id3 header_t from the song file
    header_t *header = getHeader();
    printHeader(header);
    frame_t *frame = getNextFrame();
    printFrame(frame);
    frame = getNextFrame();
    printFrame(frame);
}

void printHeader(header_t *header) {
printf("Header id: %s version: %d revision: %d unsynchronized: %d extended: %d experimental: %d size: %d\n",
            header->id, header->version, header->revision, header->unsynchronized, 
            header->extended, header->experimental, header->size);

}

void printFrame(frame_t *frame) {
    frameHeader_t *header = frame->header;
    printf("Frame Header id: %s Size: %d tagPreserve: %d filePreserver: %d readOnly: %d compressed: %d encryption: %d group %d\n", header->id, header->size, 
            header->tagPreservation, header->filePreservation, header->readOnly, header->compressed, header->encryption, header->group);
    printf("Frame Contents %s\n", frame->attribute);
}

// Current Position is the current position in the file
void increasePos(int n) {
    currentPosition += n;    
}

void printUsage() {
    DieWithError("Usage: %s mp3File");
}

void DieWithError(char *errMsg) {
    fprintf(stderr, "Error: %s\n", errMsg);
    exit(1);
}

// Memcpy wrapper that increments the position pointer
void copyTo(void *dst, size_t size) {
    memcpy(dst, file + currentPosition, size);
    increasePos(size);  
}


header_t *getHeader() {
    rawHeader_t *rawHeader = malloc(sizeof(rawHeader_t));
    header_t *header = malloc(sizeof(header_t));
    // Copies the header data into the header struct
    copyTo(rawHeader, sizeof(rawHeader_t));

    memcpy(header->id, rawHeader->id, sizeof(char) * 3);
    header->id[3] = '\0'; 

    if (strcmp(header->id, "ID3") != 0) {
        DieWithError("Not an ID3 tag!");    
    }

    // Get the version/revision data
    header->version = rawHeader->version;
    header->revision = rawHeader->revision;

    // Check for unsychronized, extended, and experimental flags
    header->unsynchronized = ((rawHeader->flags & 0x80) >> 7);
    header->extended = ((rawHeader->flags & 0x40) >> 6);
    header->experimental = ((rawHeader->flags & 0x20) >> 5);

    // if any other flag bits are set, it's an error
    if (rawHeader->flags & 0x1F) {
        DieWithError("Invalid flag bits are set!");
    }

    // Get the size. No idea why they chose this 
    // Basically they store the size in 4 bytes but you only actually count the last 3 bits of every nibble
    // Soo it's 28 bits stored in 32 bits of space
    // It's mystifying really
    unsigned int size = 0;

    for (int i = 0; i < 4; i++) {
       // printf("%x ", rawHeader->size[i]);
        unsigned int value = (rawHeader->size[i] & 0x7F); 
        value <<= (21 - i * 7);
        size |= value;
    }
    header->size = size;

    free(rawHeader);
    return header;
}

frame_t *getNextFrame() {
    rawFrameHeader_t *rawHeader = malloc(sizeof(rawFrameHeader_t));
    frameHeader_t *header = malloc(sizeof(frameHeader_t));
    frame_t *frame = malloc(sizeof(frame));

    // Copy the data from the song to the raw frame header
    copyTo(rawHeader, sizeof(rawFrameHeader_t));

    memcpy(header->id, rawHeader->id, sizeof(char) * 4);
    header->id[4] = '\0'; 

    // Get the size
    header->size = htonl(*((unsigned int *) rawHeader->size));    
    
    // Check for unsychronized, extended, and experimental flags
    header->tagPreservation = ((rawHeader->flags[0] & 0x80) >> 7);
    // Check to see if the file should be preserved 
    header->filePreservation = ((rawHeader->flags[0] & 0x40) >> 6);;
    // Is the file read only?
    header->readOnly =  ((rawHeader->flags[0] & 0x20) >> 5);
    // Is the frame compressed?
    header->compressed = ((rawHeader->flags[1] & 0x80) >> 7);
    // Encryption
    header->encryption = ((rawHeader->flags[1] & 0x40) >> 6);
    // Is the frame part of a group
    header->group = ((rawHeader->flags[1] & 20) >> 5);

    frame->header = header;
    free(rawHeader);
    
    // Get the attribute within the frame
    //frame->attribute = malloc(header->size + 1);
    //copyTo(frame->attribute, header->size);

    unsigned char *attribute = malloc(header->size + 1);
    copyTo(attribute, header->size);

    // Save the album art to disk
    if (strcmp(header->id, "APIC") == 0) {
        char encoding;
        copyTo(&encoding, 1);
        if (encoding != 0) {
            // Not sure what to do with unicode here yet
            exit(1);
        }

        // Have to get the length of the new file first
        int mimeLen = strlen((char *) file);
        char *mimeType = malloc(mimeLen + 1);
        // Add 1 to copy the null byte as well
        copyTo(mimeType, mimeLen + 1);

        unsigned char pictureType;
        copyTo(&pictureType, 1);

        // Get the description
        int descriptionLen = strlen((char *) file);
        char *description = malloc(descriptionLen + 1);
        // Add 1 to copy the null byte as well
        copyTo(description, descriptionLen + 1);

        

        
    }
    // Skip leading  0's
    for (int i = 0; attribute[i] == 0 &&  i < header->size; i++) {
        attribute++;
    }
   
    // Attribute is in utf_16 Big Endian 
    if (attribute[0] == 0x1 && attribute[1] == 0xFF && attribute[2] == 0xFE) {
        attribute += 3;        
        int newSize = (header->size - 3)/2;
        char *newAttribute = malloc(newSize + 1);
        for (int i = 0; i < header->size - 3; i++) {
           // Copy odd characters to the string
           if (i % 2 == 0) {
                newAttribute[i/2] = attribute[i];
           }
        }
        for (int i = 0; i < newSize; i++) {
        }
        newAttribute[newSize] = '\0';
        free(attribute - 3);
        frame->attribute = newAttribute;
    }
    else {
        attribute[header->size] = '\0';
        frame->attribute = (char *)attribute;
    }

    return frame;
}

void errExit(char *errMsg, ...) {
    va_list args;
    va_start(args, errMsg);
    vfprintf(stderr, errMsg, args);
    fprintf(stderr, "\n");
    exit(1);
}
