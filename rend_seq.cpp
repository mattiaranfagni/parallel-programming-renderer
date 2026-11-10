#include <iostream>
#include <fstream>
#include <vector>
#include <omp.h>
#include <random>

// Non ottimizzata visto che lavoro su architetture a 64bit, quindi viene pure diviso male nei data bus
struct Color {
    unsigned short int r, //0-255 -> 16 bit per channel -> 2 byte
    unsigned short int g, //0-255 -> 16 bit per channel -> 2 byte
    unsigned short int b; //0-255 -> 16 bit per channel -> 2 byte
    float alpha;          // 0.0f-0.1f -> 32 bit        -> 4 byte
};


// Sono 10 byte colore + 4 byte per x + 4 byte per y + 4 byte per radius + 4 byte per z = 26 byte per cerchio (non ottimizzato, comunque allargo per cache line)
struct Circle {
    float x;      // Centro X -> coordinate del centro del cerchio -> 4 byte
    float y;      // Centro Y -> coordinate del centro del cerchio -> 4 byte
    unsigned short int   z;      // Z-indexing                     -> 2 byte       
    float radius; // Raggio -> dimensione cerchio                  -> 4 byte 
    Color color;  // Colore del cerchio                            -> 10 byte (3*2 + 4)
};

// Configurazione dell'immagine di output
constexpr int WIDTH = 800;
constexpr int HEIGHT = 800;
constexpr int MAX_SIZE = 100;
constexpr int N_CIRLES_GENERATED = 1000;

int main() {
    //inizializzazione programma per inputs

    std::random_device rd;
    const unsigned int idUnico = rd();
    // Generatore Rand da seed
    std::mt19937 RandGenerator(idUnico);

    // Definiamo distribuzioni uniformi random per i colori, le posizioni e le dimensioni dei cerchi
    std::uniform_int_distribution<uint8_t> rgbDistrib(0, 255);
    std::uniform_real_distribution<float> aDistrib(0.0f, 1.0f);
    std::uniform_int_distribution<unsigned short int> locationDistrib(0, std::max(WIDTH, HEIGHT));
    std::uniform_int_distribution<unsigned short int> sizeDistrib(0, MAX_SIZE);


    std::cout << "Generazione Circles da renderizzare..." << std::endl;
    
    // False Sharing
    std::vector<Circle> circles = std::vector<Circle>{};
    //parallelizzabile
    for (int i = 0; i < N_CIRLES_GENERATED; i++) {
        Circle circle = {
            .x = static_cast<float>(locationDistrib(RandGenerator)),
            .y = static_cast<float>(locationDistrib(RandGenerator)),
            .z = i,
            .radius = static_cast<float>(sizeDistrib(RandGenerator)),
            .color = Color {
                .r = rgbDistrib(RandGenerator),
                .g = rgbDistrib(RandGenerator),
                .b = rgbDistrib(RandGenerator),
                .a = i > N_CIRLES_GENERATED ? 0.0f : aDistrib(RandGenerator)
            }
        };
        circles.push_back(circle);
    }
    std::cout << "Circles generati." << std::endl;
    //per numa archs, first touch
    std::vector<Color> imageBuffer(WIDTH * HEIGHT, Color{.r = 30, .g = 30, .b = 30, .a = 1.0f}); // Sfondo grigio scuro

    std::cout << "Ordinamento dei cerchi per z-index e rendering..." << std::endl;
    
    double startTime = omp_get_wtime();
    //parallelizzabile, usare std::execution::par per esecuzione parallela
    std::sort(
        std::execution::seq, // Esecuzione sequenziale
        circles.begin(),
        circles.end(),
        [](Circle a, Circle b) { //Lambda per ordinare i cerchi in base al loro z-index (dal più grande al più piccolo)
            return a.z > b.z;
        }
    );
    
    //parallelizzabile
    for (int y = 0; y < HEIGHT; ++y) {
        for (int x = 0; x < WIDTH; ++x) {

            // Controlliamo i cerchi dal fondo in avanti (l'ultimo disegnato sta sopra)
            for (const Circle circle : circles) {
                // Equazione del cerchio: (x - cx)^2 + (y - cy)^2 <= r^2
                const float dx = static_cast<float>(x) - circle.x;
                const float dy = static_cast<float>(y) - circle.y;

                if ((dx * dx) + (dy * dy) <= (circle.radius * circle.radius)) {
                    // Pixel colpito: coloriamo il pixel nel buffer lineare
                    // Prendiamo il colore che c'è già in quel pixel (Destinazione)
                    Color currentBackground = imageBuffer[y * WIDTH + x];
                    float a = circle.color.a;
                    // Formula Alpha Blending applicata a R, G, B
                    const auto newR = static_cast<unsigned short int>(((circle.color.r * a) + (currentBackground.r * (1.0f - a))));
                    const auto newG = static_cast<unsigned short int>(((circle.color.g * a) + (currentBackground.g * (1.0f - a))));
                    const auto newB = static_cast<unsigned short int>(((circle.color.b * a) + (currentBackground.b * (1.0f - a))));

                    imageBuffer[y * WIDTH + x] = Color{
                        .r = newR,
                        .g = newG,
                        .b = newB,
                        .a = 1.0f // Lo sfondo finale rimane opaco
                    };
                }
            }

        }
    }

    double endTime = omp_get_wtime();

    std::cout << "Rendering completato in: " << (endTime - startTime) << " secondi." << std::endl;

    std::string nomeFile = std::format("render_{}.ppm", idUnico);
    
    // Salviamo l'immagine su file in formato PPM
    std::ofstream outFile(nomeFile);
    if (!outFile) {
        std::cerr << "Errore nella creazione del file di output!" << std::endl;
        return 1;
    }

    // Header del file PPM
    outFile << "P3\n" << WIDTH << " " << HEIGHT << "\n255\n";

    // Scrittura dei pixel su file
    for (const auto& pixel : imageBuffer) {
        outFile << static_cast<unsigned short int>(pixel.r) << " "
                << static_cast<unsigned short int>(pixel.g) << " "
                << static_cast<unsigned short int>(pixel.b) << "\n";
    }

    std::cout << "Immagine salvata come " << nomeFile << std::endl;
    return 0;
}
