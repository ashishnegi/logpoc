#include "pch.h"

#include <windows.h>
#include <fileapi.h>

#include <fstream>
#include <experimental/filesystem>
#include <vector>
#include <thread>

using namespace std;

std::string GetLastErrorAsString()
{
    //Get the error message, if any.
    DWORD errorMessageID = ::GetLastError();
    if (errorMessageID == 0)
        return std::string();//No error message has been recorded

    LPSTR messageBuffer = nullptr;
    size_t size = FormatMessageA(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
        NULL, errorMessageID, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (LPSTR) &messageBuffer, 0, NULL);

    std::string message(messageBuffer, size);

    //Free the buffer.
    LocalFree(messageBuffer);

    return message;
}

void write_win32(HANDLE fileHandle, const int data_in_gb, const char start_char)
{
    const int offset = 8 * 1024;
    int buffer[offset];
    auto start_time = chrono::high_resolution_clock::now();

    long long myoffset = 0;
    const long long num_records_each_thread = (data_in_gb * 1024 * ((1024 * 1024) / offset));
    for (long long i = 0; i < num_records_each_thread; ++i)
    {
        DWORD bytesWritten;
        char buffer[offset];
        bool error = WriteFile(fileHandle, buffer, sizeof(buffer), &bytesWritten, NULL);
        SetFilePointer(fileHandle, offset, 0, FILE_CURRENT);

        if (!error)
        {
            cout << GetLastErrorAsString() << endl;
        }

        ASSERT_TRUE(error);

        myoffset += offset;
    }

    auto end_time = chrono::high_resolution_clock::now();

    std::cout << "Data written : " << data_in_gb << " GB, " << 1 << " threads "
              << ", cache " << (false ? "true " : "false ") << ", size " << offset << " bytes ";
    std::cout << "Time taken: " << (end_time - start_time).count() / 1000 << " micro-secs" << std::endl;
}

// WriteThrough + no_buffering flag
// https://docs.microsoft.com/en-us/windows/win32/api/fileapi/nf-fileapi-createfilea
TEST(win32, write_preallocated_serial_writethrough_no_buffering)
{
    string filename = "file6.log";
    const int data_in_gb = 2;

    // create file before write threads start.
    auto hfile = CreateFileA(
        filename.c_str(),// Notice the L for a wide char literal
        GENERIC_READ | GENERIC_WRITE,
        0,
        NULL,
        CREATE_ALWAYS,
        FILE_FLAG_NO_BUFFERING | FILE_FLAG_WRITE_THROUGH,
        NULL);

    write_win32(hfile, data_in_gb, 'A');
    CloseHandle(hfile);

    std::remove(filename.c_str());
}

// Code from https://stackoverflow.com/questions/4011508/how-to-create-a-sparse-file-on-ntfs
HANDLE CreateSparseFile(LPCTSTR lpSparseFileName)
{
    // Use CreateFile as you would normally - Create file with whatever flags
    //and File Share attributes that works for you
    DWORD dwTemp;

    HANDLE hSparseFile = CreateFile(lpSparseFileName,
        GENERIC_READ | GENERIC_WRITE,
        FILE_SHARE_READ | FILE_SHARE_WRITE,
        NULL,
        CREATE_ALWAYS,
        FILE_ATTRIBUTE_NORMAL,
        NULL);

    if (hSparseFile == INVALID_HANDLE_VALUE)
        return hSparseFile;

    DeviceIoControl(hSparseFile,
        FSCTL_SET_SPARSE,
        NULL,
        0,
        NULL,
        0,
        &dwTemp,
        NULL);
    return hSparseFile;
}

DWORD SetSparseRange(HANDLE hSparseFile, LONGLONG start, LONGLONG size)
{
    // Specify the starting and the ending address (not the size) of the
    // sparse zero block
    FILE_ZERO_DATA_INFORMATION fzdi;
    fzdi.FileOffset.QuadPart = start;
    fzdi.BeyondFinalZero.QuadPart = start + size;
    // Mark the range as sparse zero block
    DWORD dwTemp;
    SetLastError(0);
    BOOL bStatus = DeviceIoControl(hSparseFile,
        FSCTL_SET_ZERO_DATA,
        &fzdi,
        sizeof(fzdi),
        NULL,
        0,
        &dwTemp,
        NULL);
    if (bStatus)
        return 0;//Sucess
    else
    {
        DWORD e = GetLastError();
        return (e);//return the error value
    }
}

TEST(win32, write_preallocated_file_serial_sprase)
{
    string filename = "file8.log";
    const int data_in_gb = 2;

    auto hfile = CreateSparseFile(L"file8.log");
    SetSparseRange(hfile, 0, data_in_gb * 1024 * 1024 * 1024);

    write_win32(hfile, data_in_gb, 'A');

    CloseHandle(hfile);

    std::remove(filename.c_str());
}