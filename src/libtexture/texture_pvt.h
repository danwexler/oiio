/////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2008 Larry Gritz.
// 
// Permission is hereby granted, free of charge, to any person obtaining a
// copy of this software and associated documentation files (the
// "Software"), to deal in the Software without restriction, including
// without limitation the rights to use, copy, modify, merge, publish,
// distribute, sublicense, and/or sell copies of the Software, and to
// permit persons to whom the Software is furnished to do so, subject to
// the following conditions:
// 
// The above copyright notice and this permission notice shall be included
// in all copies or substantial portions of the Software.
// 
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
// OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
// MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
// NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE
// LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION
// OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
// WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
// 
// (This is the MIT open source license.)
/////////////////////////////////////////////////////////////////////////////


#ifndef TEXTURE_PVT_H
#define TEXTURE_PVT_H


namespace OpenImageIO {
namespace pvt {

class TextureSystemImpl;


enum TexFormat {
    TexFormatUnknown, TexFormatTexture, TexFormatTexture3d,
    TexFormatShadow, TexFormatCubeFaceShadow, TexFormatVolumeShadow,
    TexFormatLatLongEnv, TexFormatCubeFaceEnv,
    TexFormatLast
};



enum CubeLayout {
    CubeUnknown,
    CubeThreeByTwo,
    CubeOneBySix,
    CubeLast
};



/// Unique in-memory record for each texture file on disk.
///
class TextureFile {
public:
    TextureFile (TextureSystemImpl &texsys, ustring filename);
    ~TextureFile ();

    bool broken () const { return m_broken; }
    int levels () const { return (int)m_spec.size(); }
    const ImageIOFormatSpec & spec (int level=0) const { return m_spec[level]; }
    TexFormat textureformat () const { return m_texformat; }
    TextureOptions::Wrap swrap () const { return m_swrap; }
    TextureOptions::Wrap twrap () const { return m_twrap; }

private:
    ustring m_filename;             ///< Filename
    bool m_used;                    ///< Recently used (in the LRU sense)
    bool m_broken;                  ///< has errors; can't be used properly
    shared_ptr<ImageInput> m_input; ///< Open ImageInput, NULL if closed
    std::vector<ImageIOFormatSpec> m_spec;  ///< Format for each MIP-map level
    TexFormat m_texformat;          ///< Which texture format
    TextureOptions::Wrap m_swrap;   ///< Default wrap modes
    TextureOptions::Wrap m_twrap;   ///< Default wrap modes
    Imath::M44f m_Mlocal;           ///< shadows: world-to-local (light) matrix
    Imath::M44f m_Mproj;            ///< shadows: world-to-pseudo-NDC
    Imath::M44f m_Mtex;             ///< shadows: world-to-pNDC with camera z
    Imath::M44f m_Mras;             ///< shadows: world-to-raster with camera z
    CubeLayout m_cubelayout;        ///< cubemap: which layout?
    bool m_y_up;                    ///< latlong: is y "up"? (else z is up)
    TextureSystemImpl &m_texsys;    ///< Back pointer for texture system
};



/// Reference-counted pointer to a TextureFile
///
typedef shared_ptr<TextureFile> TextureFileRef;


/// Map file names to texture file references
///
typedef hash_map<ustring,TextureFileRef,ustringHash> FilenameMap;



/// Compact identifier for a particular tile of a particular texture.
///
class TileID {
public:
    /// Default constructor -- just make sure the texfile pointer is
    /// not dangling.
    TileID () : m_texfile(NULL) { }

    /// Initialize a TileID based on full elaboration of texture file,
    /// MIPmap level, and tile x,y,z indices.
    TileID (TextureFile *texfile, int level, int x, int y, int z=0)
        : m_texfile(texfile), m_level(level), m_x(x), m_y(y), m_z(z)
    { }

    /// Destructor is trivial, because we don't hold any resources
    /// of our own.  This is by design.
    ~TileID () { }

    TextureFile *texfile (void) const { return m_texfile; }
    int level (void) const { return m_level; }
    int x (void) const { return m_x; }
    int y (void) const { return m_y; }
    int z (void) const { return m_z; }

    /// Do the two ID's refer to the same tile?  
    ///
    friend bool equal (const TileID &a, const TileID &b) {
        // Try to speed up by comparing field by field in order of most
        // probable rejection if they really are unequal.
        return (a.m_x == b.m_x && a.m_y == b.m_y && a.m_z == b.m_z &&
                a.m_level == b.m_level && a.m_texfile == b.m_texfile);
    }

    /// Do the two ID's refer to the same tile, given that the
    /// caller *guarantees* that the two tiles point to the same
    /// file and level (so it only has to compare xyz)?
    friend bool equal_same_level (const TileID &a, const TileID &b) {
        DASSERT (a.m_texfile == b.m_texfile && a.m_level == b.m_level);
        return (a.m_x == b.m_x && a.m_y == b.m_y && a.m_z == b.m_z);
    }

    /// Do the two ID's refer to the same tile?  
    ///
    bool operator== (const TileID &b) const { return equal (*this, b); }

private:
    TextureFile *m_texfile;   ///< Which TextureFile we refer to
    int m_level;              ///< MIP-map level
    int m_x, m_y, m_z;        ///< x,y,z tile index within the mip level
};




class Tile {
public:
    Tile (const TileID &id);

    ~Tile ();

    /// Return pointer to the floating-point texel data
    ///
    const float *data (void) const { return (const float *)m_texels[0]; }

    /// Return a pointer to the character data
    ///
    const unsigned char *bytedata (void) const {
        return (unsigned char *) m_texels[0];
    }

    const TileID& id (void) const { return m_id; }

    /// load new data into tile from an ImageIO
    ///
    bool read_tile (OpenImageIO::ImageInput &input, int x, int y, int z,
                    ParamBaseType format=PT_FLOAT) {
        return input.read_tile (x, y, z, format, &m_texels[0]);
    }

private:
    TileID m_id;                  ///< ID of this tile
    std::vector<char> m_texels;   ///< The texel data
    bool m_valid;                 ///< Valid texels
    bool m_used;                  ///< Used recently
    float m_mindepth, m_maxdepth; ///< shadows only: min/max depth of the tile
};



/// Reference-counted pointer to a Tile
/// 
typedef shared_ptr<Tile> TileRef;



/// Working implementation of the abstract TextureSystem class.
///
class TextureSystemImpl : public TextureSystem {
public:
    TextureSystemImpl (void) { }
    virtual ~TextureSystemImpl () { };

    // Set options
    virtual void max_open_files (int nfiles) { m_max_open_files = nfiles; }
    virtual void max_memory_MB (float size) {
        m_max_memory_MB = size;
        m_max_memory_bytes = (int)(size * 1024 * 1024);
    }
    virtual void searchpath (const ustring &path) { m_searchpath = path; }
    virtual void worldtocommon (const float *mx) {
        m_Mw2c = *(Imath::M44f *)mx;
        m_Mc2w = m_Mw2c.inverse();
    }

    /// Close everything, free resources, start from scratch.
    ///
    virtual void clear () { }

    // Retrieve options
    virtual int max_open_files () const { return m_max_open_files; }
    virtual float max_memory_MB () const { return m_max_memory_MB; }
    virtual ustring searchpath () const { return m_searchpath; }
    virtual void get_commontoworld (Imath::M44f &result) const {
        result = m_Mc2w;
    }

    /// Retrieve filtered texture lookups
    ///
    virtual void texture (ustring filename, TextureOptions &options,
                          Runflag *runflags, int firstactive, int lastactive,
                          VaryingRef<float> s, VaryingRef<float> t,
                          VaryingRef<float> dsdx, VaryingRef<float> dtdx,
                          VaryingRef<float> dsdy, VaryingRef<float> dtdy,
                          float *result);

    /// Retrieve 3D filtered texture lookup
    ///
    virtual void texture (ustring filename, TextureOptions &options,
                          Runflag *runflags, int firstactive, int lastactive,
                          VaryingRef<Imath::V3f> P,
                          VaryingRef<Imath::V3f> dPdx,
                          VaryingRef<Imath::V3f> dPdy,
                          float *result) { }

    /// Retrieve a shadow lookup for position P.
    ///
    virtual void shadow (ustring filename, TextureOptions &options,
                         Runflag *runflags, int firstactive, int lastactive,
                         VaryingRef<Imath::V3f> P,
                         VaryingRef<Imath::V3f> dPdx,
                         VaryingRef<Imath::V3f> dPdy,
                         float *result) { }

    /// Retrieve an environment map lookup for direction R.
    ///
    virtual void environment (ustring filename, TextureOptions &options,
                              short *runflags, int firstactive, int lastactive,
                              VaryingRef<Imath::V3f> R,
                              VaryingRef<Imath::V3f> dRdx,
                              VaryingRef<Imath::V3f> dRdy,
                              float *result) { }

    /// Get information about the given texture.
    virtual bool gettextureinfo (ustring filename, ustring dataname,
                                 ParamType datatype, void *data);

private:
    void init ();

    /// Find the TextureFile record for the named texture, or NULL if no
    /// such file can be found.
    TextureFile *find_texturefile (ustring filename);

    /// Find a tile identified by 'id' in the tile cache, paging it in
    /// if needed, and return a reference to the tile.  Return a NULL
    /// tile ref if such tile exists in the file.
    TileRef find_tile (const TileID &id);

    /// Find the tile specified by id and place its reference in 'tile'.
    /// Use tile and lasttile as a 2-item cache of tiles to boost our
    /// hit rate over the big cache.  This is just a wrapper around
    /// find_tile(id) and avoids looking to the big cache (and locking)
    /// most of the time for fairly coherent tile access patterns.
    /// If tile is null, so is lasttile.
    void find_tile (const TileID &id,
                    TileRef &tile, TileRef &lasttile) {
        if (tile) {
            if (tile->id() == id)
                return;    // already have the tile we want
            if (lasttile) {
                // Tile didn't match, maybe lasttile will?  Swap tile
                // and last tile.  Then the new one will either match,
                // or we'll fall through and replace tile.
                swap (tile, lasttile);
                if (tile->id() == id)
                    return;
            } else {
                // Tile didn't match, and there was nothing in lasttile.
                // Move tile to lasttile, then fall through to page in
                // to tile.
                lasttile = tile;
            }
        }
        tile = find_tile (id);
    }

    /// Find the tile specified by id and place its reference in 'tile'.
    /// Use tile and lasttile as a 2-item cache of tiles to boost our
    /// hit rate over the big cache.  The caller *guarantees* that tile
    /// contains a reference to a tile in the same file and MIP-map
    /// level as 'id', and so does lasttile (if it contains a reference
    /// at all).  Thus, it's a slightly simplified and faster version of
    /// find_tile and should be used in loops where it's known that we
    /// are reading several tiles from the same level.
    void find_tile_same_level (const TileID &id,
                               TileRef &tile, TileRef &lasttile) {
        if (equal_same_level (tile->id(), id))
            return;
        if (lasttile) {
            swap (tile, lasttile);
            if (equal_same_level (tile->id(), id))
                return;
        } else {
            lasttile = tile;
        }
        tile = find_tile (id);
    }

    /// Look up texture from just ONE point
    ///
    void texture_lookup (TextureFile *texfile, TextureOptions &options,
                         int index,
                         VaryingRef<float> _s, VaryingRef<float> _t,
                         VaryingRef<float> _dsdx, VaryingRef<float> _dtdx,
                         VaryingRef<float> _dsdy, VaryingRef<float> _dtdy,
                         float *result);
    


    int m_max_open_files;
    float m_max_memory_MB;
    size_t m_max_memory_bytes;
    ustring m_searchpath;
    Imath::M44f m_Mw2c;          ///< world-to-"common" matrix
    Imath::M44f m_Mc2w;          ///< common-to-world matrix
    FilenameMap m_texturefiles;  ///< Map file names to TextureFile's
    mutex m_texturefiles_mutex;  ///< Protect filename map
//    TileMap tilecache;
    
};



};  // end namespace OpenImageIO::pvt
};  // end namespace OpenImageIO


#endif // TEXTURE_PVT_H