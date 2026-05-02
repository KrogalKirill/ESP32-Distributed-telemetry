//! Модуль MQTT брокера (rumqttd v0.17)
//! Конфигурация загружается из файла rumqttd.toml

use rumqttd::{Broker, Config};
use std::fs;
use std::path::Path;

/// Загружает конфигурацию из TOML-файла
fn load_config() -> Config {
    let config_path = Path::new("rumqttd.toml");
    
    // Читаем файл
    let config_content = fs::read_to_string(config_path)
        .expect("Не удалось прочитать rumqttd.toml. Убедитесь, что файл в папке запуска.");
    
    // Десериализуем TOML в Config
    toml::from_str(&config_content)
        .expect("Ошибка парсинга rumqttd.toml. Проверьте формат файла.")
}

/// Запускает брокер в текущем потоке
pub fn run_broker() {
    let config = load_config();
    let mut broker = Broker::new(config);
    
    println!("Брокер запускается на {}...", get_broker_address());
    
    if let Err(e) = broker.start() {
        eprintln!("Брокер остановился: {}", e);
    }
    else 
    {
        eprintln!("==========Брокер работает==========");
    }
}

pub fn get_broker_address() -> &'static str {
    "172.20.10.3:1883"
}