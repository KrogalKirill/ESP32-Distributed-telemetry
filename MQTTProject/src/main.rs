//! Два потока: брокер + клиент

mod broker;
mod client;

use std::thread;
use std::time::Duration;

fn main() {
    env_logger::init();
    
    // ═════ ПОТОК 1: Брокер ═════
    let broker_handle = thread::spawn(|| {
        println!("[Поток 1] Запуск брокера...");
        broker::run_broker();
    });
    
    // Ждём запуска брокера
    println!("Инициализация брокера...");
    thread::sleep(Duration::from_secs(2));
    
    // ═════ ПОТОК 2: Клиент ═════
    let client_handle = thread::spawn(|| {
        println!(" [Поток 2] Запуск клиента...");
        
        let runtime = tokio::runtime::Runtime::new().unwrap();
        runtime.block_on(async {
            let config = client::ClientConfig::default();
            client::run_client(config).await;
        });
    });
    println!("Оба потока запущены!");
    
    let _ = client_handle.join();
    let _ = broker_handle.join();
}