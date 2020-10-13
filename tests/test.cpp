#include "pch.h"

#include <windows.h>
#include <fileapi.h>

#include <fstream>
#include <experimental/filesystem>
#include <vector>
#include <thread>

using namespace std;

namespace fs = experimental::filesystem;
TEST(fstream, seekg_seekp_same)
{
    std::string filename = "file1.log";
    {
        std::fstream file(filename, fstream::in | fstream::out | fstream::trunc);
        EXPECT_TRUE(file);
        EXPECT_EQ(0, file.tellg());
        EXPECT_EQ(0, file.tellp());
        file.seekg(10);
        EXPECT_EQ(10, file.tellg());
        EXPECT_EQ(10, file.tellp());
        file.seekp(20);
        EXPECT_EQ(20, file.tellg());
        EXPECT_EQ(20, file.tellp());
    }
    std::remove(filename.c_str());
}

TEST(fstream, read_write_to_random_offset)
{
    string filename = "file2.log";
    {
        const int offsets[]{10, 100, 1000, 4000, 8000, 50000};// 8 KB
        char expected_data[]{"some_data"};
        {
            fstream file(filename, fstream::in | fstream::out | fstream::trunc | fstream::binary);
            EXPECT_TRUE(file);
            for (auto offset : offsets)
            {
                file.seekp(offset);
                file << expected_data;
                EXPECT_TRUE(file);
            }

            file.flush();
        }
        {
            fstream file(filename, fstream::in | fstream::binary);
            EXPECT_TRUE(file);
            EXPECT_EQ(0, file.tellg());

            for (auto offset : offsets)
            {
                char actual_data[sizeof(expected_data)];

                file.seekg(offset);
                file >> actual_data;
                EXPECT_STREQ(expected_data, actual_data);
            }

            // don't get any data from before offsets
            for (auto offset : offsets)
            {
                char data_before_write[10 + sizeof(expected_data)];

                file.seekg(offset - 10);
                file.read(data_before_write, sizeof(data_before_write));
                EXPECT_STREQ("", data_before_write);
                EXPECT_STREQ(expected_data, &data_before_write[10]);
                if (offset == offsets[(sizeof(offsets) / sizeof(int)) - 1])
                {
                    EXPECT_TRUE(file || file.eof());
                }
                else
                {
                    EXPECT_TRUE(file);
                }
            }
        }
    }

    std::remove(filename.c_str());
}

TEST(fstream, read_write_with_same_fstream)
{
    string filename = "file3.log";
    {
        string expected_data = "some_data";
        fstream file(filename, fstream::in | fstream::out | fstream::trunc);
        EXPECT_TRUE(file);
        file << expected_data;
        file.flush();
        // read from starting
        file.seekg(0);
        string actual_data;
        file >> actual_data;
        EXPECT_EQ(expected_data, actual_data);
    }
    std::remove(filename.c_str());
}

void write_concurrently(string filename, const int data_in_gb, const int num_threads, const char start_char,
    bool stream_cache = true)
{
    const int offset = 8 * 1024;
    const long long num_records_each_thread = (data_in_gb * 1024 * ((1024 * 1024) / (num_threads * offset)));

    {
        auto write_file_fn = [&](int index) {
            // each thread has its own handle
            fstream file_handle(filename, fstream::in | fstream::out | fstream::ate | fstream::binary);
            if (!stream_cache)
            {
                file_handle.rdbuf()->pubsetbuf(nullptr, 0);// no bufferring in fstream
            }

            file_handle.seekp(0, ios_base::beg);

            vector<char> data(offset, (char) (index + start_char));
            long long myoffset = index * offset;

            for (long long i = 0; i < num_records_each_thread; ++i)
            {
                file_handle.seekp(myoffset, ios_base::beg);
                file_handle.write(data.data(), offset);
                myoffset += num_threads * offset;
            }

            // file_handle.flush();
        };

        auto start_time = chrono::high_resolution_clock::now();
        vector<thread> writer_threads;
        for (int i = 0; i < num_threads; ++i)
        {
            writer_threads.push_back(std::thread(write_file_fn, i));
        }

        for (int i = 0; i < num_threads; ++i)
        {
            writer_threads[i].join();
        }

        auto end_time = chrono::high_resolution_clock::now();

        std::cout << "Data written : " << data_in_gb << " GB, " << num_threads << " threads "
                  << ", cache " << (stream_cache ? "true " : "false ") << ", size " << offset << " bytes ";
        std::cout << "Time taken: " << (end_time - start_time).count() / 1000 << " micro-secs" << std::endl;
    }

    {
        ifstream file(filename, fstream::in | fstream::binary);
        file.seekg(0, ios_base::end);
        EXPECT_EQ(num_records_each_thread * num_threads * offset, file.tellg());
        file.seekg(0, ios_base::beg);
        EXPECT_TRUE(file);

        char data[offset]{0};
        for (long long i = 0; i < (num_records_each_thread * num_threads); ++i)
        {
            file.read(data, offset);
            EXPECT_TRUE(file || file.eof());// should be able to read until eof
            char expected_char = (char) ((i % num_threads) + start_char);

            bool same = true;
            for (auto & c : data)
            {
                same = same && (c == expected_char);
            }

            EXPECT_TRUE(same);
            if (!same)
            {
                std::cout << "corruption detected !!!" << std::endl;
                break;
            }

            if (file.eof())
            {
                EXPECT_EQ(num_records_each_thread * num_threads, i + 1);
                break;
            }
        }
    }
}
// analysis: Possible but multiple thread is slower because of exlsuive locking in filesystem : ExAcquireResourceExclusiveLite
//Name                                                        	Inc %	     Inc	Exc %	   Exc	Fold	                             When	  First	       Last
//module ntoskrnl << ntoskrnl!ExAcquireResourceExclusiveLite >> 11.5	   1, 030	 11.5	 1, 030	   3	 1o010___110110110___11011011____	 63.097	 12, 941.694
//+ module ntfs.sys << ntfs!NtfsCopyWriteA >> 7.7	     693	  0.0	     0	   0	 00000___00000010o___01o10000____	 63.097	 12, 941.694
//| +module fltmgr.sys << fltmgr!FltpFastIoWrite >> 7.7	     693	  0.0	     0	   0	 00000___00000010o___01o10000____	 63.097	 12, 941.694
//| +module ntoskrnl << ntoskrnl!NtWriteFile >> 7.7	     693	  0.0	     0	   0	 00000___00000010o___01o10000____	 63.097	 12, 941.694
//+ module ntfs.sys << ntfs!NtfsFsdWrite >> 3.8	     337	  0.0	     0	   0	 o0000___00o00o00o___00o00o00____	 68.759	 12, 912.626
//+ module ntoskrnl << ntoskrnl!IofCallDriver >> 3.8	     337	  0.0	     0	   0	 o0000___00o00o00o___00o00o00____	 68.759	 12, 912.626
//+ module fltmgr.sys << fltmgr!FltpDispatch >> 3.8	     337	  0.0	     0	   0	 o0000___00o00o00o___00o00o00____	 68.759	 12, 912.626
//+ module ntoskrnl << ntoskrnl!NtWriteFile >> 3.8	     337	  0.0	     0	   0	 o0000___00o00o00o___00o00o00____	 68.759	 12, 912.626

TEST(fstream, write_concurrently_to_same_file)
{
    string filename = "file4.log";

    {
        // create file before write threads start.
        fstream file(filename, fstream::in | fstream::out | fstream::trunc | fstream::binary);
    }

    write_concurrently(filename, 2, 4, 'A');// concurrent is slower than 1 thread

    std::remove(filename.c_str());
}

// Preallocated file
TEST(fstream, preallocated_file_concurrent_writes)
{
    string filename = "file5.log";
    const int data_in_gb = 2;
    {
        // create file before write threads start.
        fstream file(filename, fstream::in | fstream::out | fstream::trunc | fstream::binary);
        write_concurrently(filename, data_in_gb, 1, 'A');
    }

    std::cout << "Preallocated file 5." << std::endl;
    write_concurrently(filename, data_in_gb, 1, 'B');
    write_concurrently(filename, data_in_gb, 4, 'C');

    std::remove(filename.c_str());
}

// Preallocated file + no stream cache
TEST(fstream, preallocated_file_concurrent_writes_stream_cache)
{
    string filename = "file5.log";
    const int data_in_gb = 8;
    {
        // create file before write threads start.
        fstream file(filename, fstream::in | fstream::out | fstream::trunc | fstream::binary);
        write_concurrently(filename, data_in_gb, 1, 'A');
    }

    std::cout << "Preallocated file 2." << std::endl;
    write_concurrently(filename, data_in_gb, 1, 'B', false);
    write_concurrently(filename, data_in_gb, 4, 'C', false);

    std::remove(filename.c_str());
}

// WriteThrough + no_buffering flag
// https://docs.microsoft.com/en-us/windows/win32/api/fileapi/nf-fileapi-createfilea
TEST(fstream, preallocated_file_concurrent_writes_writethrough_no_buffering)
{
    string filename = "file6.log";
    const int data_in_gb = 8;
    {
        // create file before write threads start.
        auto hfile = CreateFileA(
            filename.c_str(),// Notice the L for a wide char literal
            GENERIC_READ | GENERIC_WRITE,
            0,
            NULL,
            CREATE_ALWAYS,
            FILE_FLAG_NO_BUFFERING | FILE_FLAG_WRITE_THROUGH,
            NULL);
        CloseHandle(hfile);
    }

    std::cout << "Preallocated file 2." << std::endl;
    write_concurrently(filename, data_in_gb, 1, 'B', false);
    write_concurrently(filename, data_in_gb, 4, 'C', false);

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

// Sparse file
// https://docs.microsoft.com/en-us/windows/win32/fileio/sparse-file-operations
TEST(fstream, preallocated_file_concurrent_writes_sprase_stl)
{
    string filename = "file7.log";
    const int data_in_gb = 8;
    {
        // create file before write threads start.
        fstream file(filename, fstream::in | fstream::out | fstream::trunc | fstream::binary);
        fs::path p = fs::current_path() / filename;
        fs::resize_file(p, data_in_gb * 1024 * 1024 * 1024);
    }

    write_concurrently(filename, 8, 4, 'A');// concurrent is slower than 1 thread
    write_concurrently(filename, 8, 1, 'A');// concurrent is slower than 1 thread

    std::remove(filename.c_str());
}


TEST(fstream, preallocated_file_concurrent_writes_sprase)
{
    string filename = "file8.log";
    const int data_in_gb = 8;
    {
        auto hfile = CreateSparseFile(L"file8.log");
        SetSparseRange(hfile, 0, data_in_gb * 1024 * 1024 * 1024);
        SetEndOfFile(hfile);
        CloseHandle(hfile);
    }

    write_concurrently(filename, 8, 4, 'A');// concurrent is slower than 1 thread
    write_concurrently(filename, 8, 1, 'A');// concurrent is slower than 1 thread

    std::remove(filename.c_str());
}
