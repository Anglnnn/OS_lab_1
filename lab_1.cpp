#include <iostream>

#include <netinet/in.h>

#include <pthread.h>

#include <string>

#include <sys/socket.h>

#include <sys/wait.h>

#include <thread>

#include <unistd.h>

struct ComputationResult {
  bool success;
  bool criticalFailure;
  std::string value;
  std::string reason;
};

ComputationResult computeF(int x) {
  return {
    true,
    false,
    std::to_string(x * 2),
    ""
  };
}

ComputationResult computeG(int x) {
  return {
    true,
    false,
    std::to_string(x * 2),
    ""
  };
}

pthread_mutex_t mutex; // Mutex for synchronization

void manager(int x, int clientSocket) {
  // Запускаємо обчислення f в окремому процесі
  pid_t pidF = fork();

  if (pidF == 0) {
    // В процесі f
    ComputationResult resultF = computeF(x);

    // Захоплюємо м'ютекс перед записом в спільну пам'ять
    pthread_mutex_lock( & mutex);

    // Записуємо результат f в спільну пам'ять
    std::string resultFValue = resultF.value;

    // Звільняємо м'ютекс після запису в спільну пам'ять
    pthread_mutex_unlock( & mutex);

    // Завершуємо процес f
    exit(0);
  }

  // Захоплюємо м'ютекс перед обчисленням g
  pthread_mutex_lock( & mutex);

  // Запускаємо обчислення g в поточному процесі
  ComputationResult resultG = computeG(x);

  // Звільняємо м'ютекс після обчислення g
  pthread_mutex_unlock( & mutex);

  // Чекаємо завершення обчислення f
  waitpid(pidF, NULL, 0);

  // Отримуємо результат з процесу f 

  char buffer[1024];
  recv(clientSocket, buffer, sizeof(buffer), 0);
  std::string resultFValue(buffer);

  // Логіка обробки результатів
  ComputationResult resultF = computeF(x);
  std::string finalResult;

  if (resultF.criticalFailure) {
    std::cout << "Відмова в обчисленні f. Причина: " << resultF.reason << "\n";
  } else {
    // Concatenate resultF and resultG directly
    finalResult = resultF.value + resultG.value;

    if (resultF.success && !resultF.criticalFailure) {
      std::cout << "Значення виразу: " << finalResult << "\n";
    } else {
      std::cout << "Відмова в обчисленні. Причина: ";
      if (!resultF.success || resultF.criticalFailure) {
        std::cout << "f";
      }
      if (!resultG.success || resultG.criticalFailure) {
        if (!resultF.success || resultF.criticalFailure) {
          std::cout << " та g";
        } else {
          std::cout << "g";
        }
      }
      std::cout << "\n";
    }
  }

  // Send the final result to the client
  send(clientSocket, finalResult.c_str(), finalResult.size(), 0);

  // Close the client socket
  close(clientSocket);
}

void cancelComputation() {
  std::cout << "Обчислення скасовано. Причина: " <<
    "" <<
    "\n";
}

int main() {
  int x;
  std::cout << "Введіть значення x: ";
  std::cin >> x;

  // Ініціалізуємо м'ютекс
  pthread_mutex_init( & mutex, NULL);

  // Створюємо сокет
  int serverSocket = socket(AF_INET, SOCK_STREAM, 0);
  if (serverSocket == -1) {
    perror("socket");
    return 1;
  }

  // Біндимо сокет
  sockaddr_in serverAddress {};
  serverAddress.sin_family = AF_INET;
  serverAddress.sin_port = htons(8080);
  serverAddress.sin_addr.s_addr = INADDR_ANY;

  if (bind(serverSocket, (struct sockaddr * ) & serverAddress,
      sizeof(serverAddress)) == -1) {
    perror("bind");
    close(serverSocket);
    return 1;
  }

  // Чекаємо на підключення
  if (listen(serverSocket, 1) == -1) {
    perror("listen");
    close(serverSocket);
    return 1;
  }

  std::cout << "Чекаємо на підключення...\n";

  sockaddr_in clientAddress {};
  socklen_t clientAddrLen = sizeof(clientAddress);

  // Приймаємо підключення
  int clientSocket =
    accept(serverSocket, (struct sockaddr * ) & clientAddress, & clientAddrLen);
  if (clientSocket == -1) {
    perror("accept");
    close(serverSocket);
    return 1;
  }

  std::cout << "Клієнт підключений.\n";

  // Запуск менеджера
  manager(x, clientSocket);

  // Закриваємо сокет та знищуємо м'ютекс
  close(serverSocket);
  pthread_mutex_destroy( & mutex);

  return 0;
}