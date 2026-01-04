import sys
import serial                         # pySerial для работы с COM‑портом
import serial.tools.list_ports       # для получения списка доступных портов
from PyQt5.QtWidgets import (        # импорт виджетов PyQt5
    QApplication, QWidget, QComboBox, QPushButton, QLineEdit,
    QVBoxLayout, QHBoxLayout, QTextEdit, QLabel
)

# Основной класс графического интерфейса
class SerialGUI(QWidget):
    def __init__(self):
        super().__init__()
        self.ser = None                # объект serial-порта (будет создан после подключения)
        self.init_ui()                 # создаем интерфейс

    # Настройка интерфейса
    def init_ui(self):
        self.setWindowTitle("PWM UART Control")  # заголовок окна

        # --- Компоненты выбора порта ---
        self.port_label = QLabel("COM Port:")    # текстовая метка
        self.port_combo = QComboBox()            # выпадающий список портов
        self.refresh_ports()                     # заполняем список доступными портами

        self.connect_btn = QPushButton("Connect")        # кнопка "Подключиться"
        self.connect_btn.clicked.connect(self.connect_serial)  # привязываем обработчик

        # --- Кнопки управления PWM ---
        self.start_btn = QPushButton("Start PWM")
        self.start_btn.clicked.connect(lambda: self.send_cmd("start"))  # старт PWM

        self.stop_btn = QPushButton("Stop PWM")
        self.stop_btn.clicked.connect(lambda: self.send_cmd("stop"))   # стоп PWM

        # --- Управление скважностью ---
        self.set_label = QLabel("Set Duty (0–100):")
        self.set_input = QLineEdit()             # поле ввода числа
        self.set_btn = QPushButton("Set")
        self.set_btn.clicked.connect(self.set_duty)  # обработчик "поставить скважность"

        # --- Управление частотой ---
        self.freq_label = QLabel("Set Freq (1000–50000):")
        self.freq_input = QLineEdit()
        self.freq_btn = QPushButton("Set Freq")
        self.freq_btn.clicked.connect(self.set_freq)  # обработчик частоты

        # --- Статус PWM ---
        self.status_btn = QPushButton("Status")
        self.status_btn.clicked.connect(lambda: self.send_cmd("status"))

        # --- Окно вывода логов/ответов от устройства ---
        self.output = QTextEdit()
        self.output.setReadOnly(True)             # сделать текст только для чтения

        # --- Раскладки (layout) интерфейса ---
        port_layout = QHBoxLayout()
        port_layout.addWidget(self.port_label)
        port_layout.addWidget(self.port_combo)
        port_layout.addWidget(self.connect_btn)

        duty_layout = QHBoxLayout()
        duty_layout.addWidget(self.set_label)
        duty_layout.addWidget(self.set_input)
        duty_layout.addWidget(self.set_btn)

        freq_layout = QHBoxLayout()
        freq_layout.addWidget(self.freq_label)
        freq_layout.addWidget(self.freq_input)
        freq_layout.addWidget(self.freq_btn)

        btn_layout = QHBoxLayout()
        btn_layout.addWidget(self.start_btn)
        btn_layout.addWidget(self.stop_btn)
        btn_layout.addWidget(self.status_btn)

        main_layout = QVBoxLayout()
        main_layout.addLayout(port_layout)
        main_layout.addLayout(btn_layout)
        main_layout.addLayout(duty_layout)
        main_layout.addLayout(freq_layout)
        main_layout.addWidget(self.output)

        self.setLayout(main_layout)  # применяем главный layout

    # Получить и обновить список COM‑портов
    def refresh_ports(self):
        self.port_combo.clear()
        ports = serial.tools.list_ports.comports()  # получаем список портов
        for p in ports:
            self.port_combo.addItem(p.device)       # добавляем имя порта

    # Обработчик кнопки "Connect" — открывает выбранный порт
    def connect_serial(self):
        port = self.port_combo.currentText()        # получаем выбранный порт
        try:
            # открываем сериал с параметрами
            self.ser = serial.Serial(port, 115200, timeout=1)
            self.output.append(f"Connected to {port}")  # выводим в окно
        except Exception as e:
            self.output.append(f"Connection error: {e}")  # если ошибка

    # Отправка команды по UART
    def send_cmd(self, cmd):
        # проверяем, открыт ли порт
        if not self.ser or not self.ser.is_open:
            self.output.append("📌 Not connected!")
            return

        try:
            # отправляем команду + перевод строки
            self.ser.write((cmd.strip() + "\n").encode())
            self.output.append(f"> {cmd}")  # показываем в лог

            # пробуем считать один ответ (если устройство отвечает)
            response = self.ser.readline().decode(errors="ignore").strip()
            if response:  # если что‑то пришло
                self.output.append(f"< {response}")
        except Exception as e:
            self.output.append(f"UART write error: {e}")  # вывод ошибки

    # Обработчик кнопки установки скважности
    def set_duty(self):
        val = self.set_input.text().strip()
        if val.isdigit():  # проверяем, что строка — число
            self.send_cmd(f"set {val}")  # формируем команду
        else:
            self.output.append("⚠ Invalid duty")

    # Обработчик кнопки установки частоты
    def set_freq(self):
        val = self.freq_input.text().strip()
        if val.isdigit():
            self.send_cmd(f"freq {val}")
        else:
            self.output.append("⚠ Invalid freq")


# Запуск приложения
if __name__ == "__main__":
    app = QApplication(sys.argv)   # создаем Qt‑приложение
    gui = SerialGUI()              # создаем наше окно
    gui.show()                     # показываем его
    sys.exit(app.exec_())          # запускаем цикл обработки событий
