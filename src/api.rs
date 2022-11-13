use std::error::Error;
use std::{fs, io};
use std::path::Path;

extern crate zip;

// pub(crate) fn get_text() -> Result<String, Box<dyn Error>> {
//     let response = reqwest::blocking::get("https://maksq.ru")?;
//     if response.status().is_success() {
//         Ok(response.text()?)
//     } else {
//         Err(Box::new(io::Error::new(
//             io::ErrorKind::Other,
//             format!("Error: {}", response.status()),
//         )))
//     }
// }


fn get_file_url(value: String) -> Result<String, Box<dyn Error>> {
    let dl = r#"{
          "ultrafuck": "https://cdn.discordapp.com/attachments/706873425821565029/1041107719647658084/UltraFuck_2.36.zip",
          "moonloader": "https://cdn.discordapp.com/attachments/706873425821565029/1041110146442596513/moonloader_0.26.zip",
          "sampfuncs": "https://cdn.discordapp.com/attachments/706873425821565029/1041110530905100518/SAMPFUNCS_5.4.1.zip",
          "cleo": "https://cdn.discordapp.com/attachments/706873425821565029/1041110676128677898/CLEO.zip",
          "asi": "https://cdn.discordapp.com/attachments/706873425821565029/1041110542535893053/silents_asi_loader_13.zip"
    }"#;
    let json: serde_json::Value =
        serde_json::from_str(dl).expect("Error: Can't parse json");
    Ok(json[value].to_string().replace("\"", ""))
}

pub(crate) fn download_file(path: &str, name: &str) {
    let url = get_file_url(name.replace(".zip", "")).unwrap();
    println!("Downloading {} to {}", url, path);
    download_file_by_reqwest(&url, path, name).unwrap()
}

pub(crate) fn unzip_file(path_: &str, name: &str) {
    let path = format!("{}\\{}", path_, name);
    let path = Path::new(&path);
    let file = fs::File::open(path).unwrap();
    let mut archive = zip::ZipArchive::new(file).unwrap();
    for i in 0..archive.len() {
        let mut file = archive.by_index(i).unwrap();
        let outpath = path_.to_string() + "\\" + file.name();
        if (&*file.name()).ends_with('/') {
            fs::create_dir_all(&outpath).unwrap();
        } else {
            let mut outfile = fs::File::create(&outpath).unwrap();
            io::copy(&mut file, &mut outfile).unwrap();
        }
    }
}

fn download_file_by_reqwest(url: &str, path: &str, name: &str) -> Result<(), Box<dyn Error>> {
    let mut response = reqwest::blocking::get(url)?;
    let mut file = fs::File::create(format!("{}\\{}", path, name))?;
    io::copy(&mut response, &mut file)?;

    unzip_file(path, name);

    fs::remove_file(format!("{}\\{}", path, name)).expect("Error: Can't delete file");

    println!("File {} downloaded successfully", name);
    Ok(())
}