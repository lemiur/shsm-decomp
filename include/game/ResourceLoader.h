#ifndef GAME_RESOURCELOADER_H
#define GAME_RESOURCELOADER_H

/*
 * The asset loader's static texture sharing (ShareTextures__14ResourceLoaderFb, unit at 0x802045CC
 * not yet reconstructed). Its other members are not yet recovered. The class name is dtk's
 * signature name.
 */
class ResourceLoader {
public:
    // defined in 0x802067DC, unit not yet reconstructed: sets the texture sharing switch
    static void    ShareTextures( bool share );
};

#endif
