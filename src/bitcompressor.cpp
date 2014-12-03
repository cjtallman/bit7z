#include "../include/bitcompressor.hpp"

#include "7zip/Archive/IArchive.h"
#include "Common/StringConvert.h"
#include "Windows/COM.h"
#include "Windows/FileFind.h"
#include "Windows/PropVariant.h"

#include "../include/updatecallback.hpp"
#include "../include/bitexception.hpp"
#include "../include/bitfilesystem.hpp"

using namespace Bit7z;
using namespace Bit7z::FileSystem;
using namespace NWindows;

BitCompressor::BitCompressor( const Bit7zLibrary& lib, BitFormat format ) : mLibrary( lib ),
    mFormat( format ), mPassword( L"" ), mCryptHeaders( false ), mSolidMode( false ) {}

void BitCompressor::setPassword( const wstring& password, bool crypt_headers ) {
    mPassword = password;
    mCryptHeaders = crypt_headers;
}

void BitCompressor::compressFile( const wstring& in_file, const wstring& out_archive ) {
    compressFiles( {in_file}, out_archive );
}

void BitCompressor::compressFiles( const vector<wstring>& in_files, const wstring& out_archive ) {

    CObjectVector<CDirItem> dirItems;
    int i;

    for ( i = 0; i < in_files.size(); i++ ) {
        CDirItem di;
        UString name = in_files[i].c_str();
        NFile::NFind::CFileInfoW fi;

        if ( !fi.Find( name ) ) throw BitException( UString( L"Can't find file " ) + name );

        di.Attrib = fi.Attrib;
        di.Size = fi.Size;
        di.CTime = fi.CTime;
        di.ATime = fi.ATime;
        di.MTime = fi.MTime;
        di.Name = in_files[i].substr( in_files[i].find_last_of( L"/\\" ) + 1 ).c_str();
        di.FullPath = name;
        dirItems.Add( di );
    }

    CMyComPtr<IOutArchive> outArchive = mLibrary.outputArchiveObject( mFormat );
    if ( mCryptHeaders ) {
        const wchar_t* names[] = {L"he"};
        const int kNumProps = sizeof( names ) / sizeof( names[0] );
        NWindows::NCOM::CPropVariant values[kNumProps] = {
            true     // crypted headers ON
        };
        CMyComPtr<ISetProperties> setProperties;
        if ( outArchive->QueryInterface( IID_ISetProperties, ( void** )&setProperties ) != S_OK )
            throw BitException( "ISetProperties unsupported" );
        if ( setProperties->SetProperties( names, values, kNumProps ) != S_OK )
            throw BitException( "Cannot set properties of the archive" );
    }
    COutFileStream* outFileStreamSpec = new COutFileStream;
    if ( !outFileStreamSpec->Create( out_archive.c_str(), false ) )
        throw BitException( "Can't create archive file" );
    UpdateCallback* updateCallbackSpec = new UpdateCallback;
    CMyComPtr<IArchiveUpdateCallback2> updateCallback( updateCallbackSpec );
    updateCallbackSpec->Init( &dirItems );
    updateCallbackSpec->mIsPasswordDefined = mPassword.size() > 0;
    updateCallbackSpec->mPassword = mPassword.c_str();
    HRESULT result = outArchive->UpdateItems( outFileStreamSpec, dirItems.Size(), updateCallback );
    updateCallbackSpec->Finilize();

    if ( result != S_OK ) throw BitException( "Update Error" );

    UString errorString = L"";
    for ( i = 0; i < updateCallbackSpec->mFailedFiles.Size(); i++ )
        errorString += ( UString )L"Error for file: " + updateCallbackSpec->mFailedFiles[i] + L"\r\n";

    if ( updateCallbackSpec->mFailedFiles.Size() != 0 )
        throw BitException( errorString );
}
