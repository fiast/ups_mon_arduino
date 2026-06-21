#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <termios.h>
#include <unistd.h>
#include <errno.h>
#include <sys/ioctl.h>

#define PORT "/dev/ttyS0"
#define BAUDRATE B57600

int main() {
    int fd;
    struct termios tty;
    char buf[256];
    int n, total = 0;

    // 1. Открываем с O_NONBLOCK, чтобы предотвратить некоторые затыки драйвера MIPS
    fd = open(PORT, O_RDWR | O_NOCTTY | O_NONBLOCK);
    if (fd < 0) {
        perror("open");
        goto error;
    }

    // Убираем флаг non-blocking для перехода к предсказуемому чтению
    fcntl(fd, F_SETFL, 0);

    memset(&tty, 0, sizeof(tty));
    if (tcgetattr(fd, &tty) != 0) {
        perror("tcgetattr");
        close(fd);
        goto error;
    }

    cfsetospeed(&tty, BAUDRATE);
    cfsetispeed(&tty, BAUDRATE);

    // Базовые настройки сырого (raw) режима
    tty.c_cflag = (tty.c_cflag & ~CSIZE) | CS8;
    tty.c_iflag &= ~(IGNBRK | BRKINT | PARMRK | ISTRIP | INLCR | IGNCR | ICRNL | IXON | IXOFF | IXANY);
    tty.c_lflag &= ~(ECHO | ECHONL | ICANON | ISIG | IEXTEN);
    tty.c_oflag &= ~OPOST;

    // Включаем чтение и игнорируем линии управления (важно для трехпроводного UART)
    tty.c_cflag |= (CLOCAL | CREAD);
    tty.c_cflag &= ~(PARENB | PARODD | CSTOPB | CRTSCTS);

    // 2. НАДЕЖНЫЙ ТАЙМАУТ НА УРОВНЕ ЯДРА (Вместо select)
    // VMIN = 0, VTIME = 20 означает: read() заблокируется и будет ждать данные.
    // Если в течение 2.0 секунд не придет вообще ничего -> read вернет 0 (таймаут).
    // Если данные придут, read вернет их сразу, не дожидаясь конца двух секунд.
    tty.c_cc[VMIN] = 0;
    tty.c_cc[VTIME] = 20; // Десятые доли секунды (20 = 2 секунды)

    if (tcsetattr(fd, TCSANOW, &tty) != 0) {
        perror("tcsetattr");
        close(fd);
        goto error;
    }

    // 3. ФИКС СБРОСА АРДУИНО: Принудительно поднимаем DTR обратно, 
    // чтобы Ардуино не думала, что её хотят прошить.
    int flags;
    if (ioctl(fd, TIOCMGET, &flags) == 0) {
        flags |= TIOCM_DTR; // Удерживаем DTR в высоком состоянии
        ioctl(fd, TIOCMSET, &flags);
    }

    // Очищаем буферы от мусора, который мог прилететь при открытии
    tcflush(fd, TCIOFLUSH);

    // Если Ардуино все же перезагружается, увеличьте эту паузу до 2000000 (2 сек)
    // Но с флагом DTR выше должно работать сразу. На всякий случай даем 250мс.
    usleep(250000); 

    // Отправляем запрос
    if (write(fd, "Q\n", 2) != 2) {
        perror("write");
        close(fd);
        goto error;
    }

    // 4. БЕЗОПАСНОЕ ПОБАЙТОВОЕ ЧТЕНИЕ С УЧЕТОМ СКОРОСТИ UART
    while (total < sizeof(buf) - 1) {
        n = read(fd, buf + total, 1);
        
        if (n < 0) {
            if (errno == EAGAIN || errno == EINTR) continue;
            break; 
        }
        if (n == 0) {
            // Если мы уже что-то прочитали, но read() вернул 0 — значит строка оборвалась.
            // Если ничего не прочитали (total == 0) — это чистый таймаут ответа.
            break; 
        }

        // Фильтруем мусор в начале строки, если он есть
        if (total == 0 && (buf[0] == '\n' || buf[0] == '\r')) {
            continue;
        }

        if (buf[total] == '\n' || buf[total] == '\r') {
            break; // Встретили конец строки от Ардуино
        }
        
        total++;
    }
    buf[total] = '\0';
    close(fd);

    if (total == 0) {
        // Сюда попадем при жестком таймауте (Ардуино молчит)
        printf("OFFLINE BATTV=0.0 LINEV=0\n");
    } else {
        // Выводим чистую строку без лишних переносов
        char *p = buf;
        while (*p == '\r' || *p == '\n') p++;
        if (strlen(p) > 0) {
            printf("%s\n", p);
        } else {
            printf("OFFLINE BATTV=0.0 LINEV=0\n");
        }
    }
    fflush(stdout);
    return 0;

error:
    printf("OFFLINE BATTV=0.0 LINEV=0\n");
    fflush(stdout);
    return 1;
}
