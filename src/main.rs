extern crate winreg;

use std::error::Error;
use std::io::stdin;
use std::process::exit;

use winreg::enums::*;
use winreg::RegKey;

use crate::api::download_file;

mod text;
mod api;


fn get_gta_path() -> Result<String, Box<dyn Error>> {
    let hklm = RegKey::predef(HKEY_CURRENT_USER);
    let samp = hklm.open_subkey("Software\\SAMP").unwrap();
    let gta_sa_exe: String = samp.get_value("gta_sa_exe").unwrap();
    Ok(gta_sa_exe.replace("\\gta_sa.exe", ""))
}

fn main() {
    let mut gta_path = get_gta_path().unwrap();
    println!("{}",
             main_message!()
                 .replace("{last_gta_path}", &gta_path)
    );

    let mut question1 = String::new();
    stdin().read_line(&mut question1).expect("Failed to read line");
    let input: i32 = question1.trim().parse().expect("Please type a number!");

    match input {
        1 => println!("{}", choose_installs!()),
        2 => {
            println!("Введите путь к GTA:SA");
            let mut path_question = String::new();
            stdin().read_line(&mut path_question).expect("Failed to read line");
            gta_path = path_question.trim().to_string();
            println!("{}", choose_installs!());
        }
        3 => exit(0),
        _ => println!("Error"),
    }

    let mut question2 = String::new();
    stdin().read_line(&mut question2).expect("Failed to read line");
    let input: i32 = question2.trim().parse().expect("Please type a number!");

    println!("\n\n-> GTA Path: {}\n\n", gta_path);
    match input {
        1 => {
            download_file(gta_path.as_str(), "ultrafuck.zip");
            download_file(gta_path.as_str(), "moonloader.zip");
            download_file(gta_path.as_str(), "sampfuncs.zip");
            download_file(gta_path.as_str(), "cleo.zip");
            download_file(gta_path.as_str(), "asi.zip");
        }
        2 => download_file(gta_path.as_str(), "ultrafuck.zip"),
        3 => download_file(gta_path.as_str(), "moonloader.zip"),
        4 => download_file(gta_path.as_str(), "sampfuncs.zip"),
        5 => download_file(gta_path.as_str(), "cleo.zip"),
        6 => download_file(gta_path.as_str(), "asi.zip"),
        7 => exit(0),
        _ => println!("Error"),
    }

    println!("\nPress enter to exit");
    let mut exit_question = String::new();
    stdin().read_line(&mut exit_question).expect("Failed to read line");
    exit(0);
}
