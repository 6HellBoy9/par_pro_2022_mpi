// Copyright 2022 Kandrin Alexey
#include "../../../modules/task_2/kandrin_a_readers_writers/readers_writers.h"

#include <mpi.h>

#include <algorithm>
#include <vector>
#include <memory>

#ifdef DEBUG_OUTPUT
#include <fstream>
extern std::vector<std::ofstream> outs;
#endif

ByteSpan::ByteSpan(char* begin, size_t size) : m_begin(begin), m_size(size) {}

const char* ByteSpan::GetData() const { return m_begin; }

size_t ByteSpan::GetSize() const { return m_size; }

Memory::Memory() { memset(m_buffer.data(), 0, m_buffer.size()); }

void Memory::Write(ByteSpan span, size_t index) {
  assert(index + span.GetSize() <= m_buffer.size());

  // we use "memmove" (instead of "memcpy") because span and buffer memory can
  // overlap
  memmove(m_buffer.data() + index, span.GetData(), span.GetSize());
}

ByteSpan Memory::Read(size_t size, size_t index) {
  assert(index + size <= m_buffer.size());
  return ByteSpan(m_buffer.data() + index, size);
}

void masterProcessFunction(Memory * memory) {
  int procCount;
  MPI_Comm_size(MPI_COMM_WORLD, &procCount);

  const int workerCount = procCount - 1;

  std::vector<MPI_Request> requests(workerCount, MPI_REQUEST_NULL);
  std::vector<OperationInt> buffers(workerCount);

  // Receive procCount - 1 requests
  for (int i = 1; i < workerCount; ++i) {
    auto& currentRequest = requests.at(i - 1);
    auto& currentBuffer = buffers.at(i - 1);
    if (currentRequest == MPI_REQUEST_NULL) {
      MPI_Irecv(reinterpret_cast<char*>(&currentBuffer), sizeof(OperationInt),
                MPI_CHAR, i, 0, MPI_COMM_WORLD, &currentRequest);
    }
  }

  outs[0] << "wait for recv" << std::endl;

  while (true) {

    for (int i = 0; i < workerCount; ++i) {
      auto& currentRequest = requests.at(i);
      auto& currentOperation = buffers.at(i);
      int isSuccess = 0;
      MPI_Status status;
      //MPI_Test(&currentRequest, &isSuccess, &status);

      MPI_Wait(&currentRequest, &status);
      if (isSuccess != 0) {
        outs[0] << "Test is success (from " << i + 1 << ")" << std::endl;
        outs[0] << "isSuccess = " << isSuccess << std::endl;
        // handle operation
        outs[0] << currentOperation << std::endl;
        outs[0] << ByteSpan(reinterpret_cast<char*>(&currentOperation),
                            sizeof(OperationInt))
                << std::endl;
        currentOperation.SetMemory(memory);
        auto result = currentOperation.Perform();
        outs[0] << "Perform is completed" << std::endl;
        if (currentOperation.GetOperationType() ==
            OperationInt::OperationType::read) {
          // send response
          MPI_Send(&result, 1, MPI_INT, i + 1, 0, MPI_COMM_WORLD);
        }

        // process the request - then accept the next request
        MPI_Irecv(reinterpret_cast<char*>(&currentOperation),
                  sizeof(OperationInt),
                  MPI_CHAR, i + 1, 0, MPI_COMM_WORLD, &currentRequest);
      }
    }
  }
}

std::vector<int> readerProcessFunction(int readingCount) {
  std::vector<int> results(readingCount, 0);
  std::vector<OperationInt> operations(
      readingCount, OperationInt(0, OperationInt::OperationType::read, 0));
  std::vector<MPI_Request> requests(readingCount);

  int rank;
  MPI_Comm_rank(MPI_COMM_WORLD, &rank);

  for (int i = 0; i < readingCount; ++i) {
    auto& currentOperation = operations.at(i);
    auto& currentRequest = requests.at(i);
    outs[rank] << currentOperation << std::endl;

    MPI_Isend(reinterpret_cast<char*>(&currentOperation), sizeof(OperationInt),
              MPI_CHAR, 0, 0, MPI_COMM_WORLD, &currentRequest);
  }

  for (int i = 0; i < readingCount; ++i) {
    auto& currentResult = results[i];

    MPI_Status status;
    MPI_Recv(&currentResult, 1, MPI_INT, 0, 0, MPI_COMM_WORLD, &status);
  }

  return results;
}

void writerProcessFunction(std::vector<OperationInt> & operations) {
  std::vector<MPI_Request> requests(operations.size());

  int rank;
  MPI_Comm_rank(MPI_COMM_WORLD, &rank);

  for (int i = 0; i < operations.size(); ++i) {
    auto& currentRequest = requests.at(i);
    auto& currentOperation = operations.at(i);
    outs[rank] << currentOperation << std::endl;
    outs[rank] << ByteSpan(reinterpret_cast<char*>(&currentOperation),
                           sizeof(OperationInt)) << std::endl;
    MPI_Isend(reinterpret_cast<char*>(&currentOperation), sizeof(OperationInt), MPI_CHAR, 0,
              0,
              MPI_COMM_WORLD, &currentRequest);
  }
  std::vector<MPI_Status> statuses(operations.size());
  MPI_Waitall(requests.size(), requests.data(), statuses.data());
}
