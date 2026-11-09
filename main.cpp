#include <iostream>
#include <fstream>
#include <vector>
#include <cmath>
#include <omp.h>
#include <random>

//heaviest 2 byte per canale + 4 byte per float = 10byte
// (non ottimizzata visto che lavoro su architetture a 64bit, quindi viene pure diviso male nei data bus)
struct ColorIntAndFloat {
    unsigned short int r{0}, g{0}, b{0};
    float alpha{0.0f};
};
//meglio ma comunque non ottimizzata, non sfrutto a pieno il data bus a 64 bit , 3 byte rgb + 4 byte = 7byte
//simd su rgb non alpha
struct ColorUIntAndFloat {
    uint8_t r{0}, g{0}, b{0};
    float a{0.0f};
};
//soluzione ottima, ho 1 byte per canale, quantizzo alpha in 256 valori di trasparenza possibile = 4byte = 32bit
//processati tutti insieme in un solo flop, elaborabile con simd istructions.
// ancora meglio => uint_fast8_t !! provare differenze
struct Color {
    uint8_t r{0}, g{0}, b{0}, a{255};
};

// Definizione del cerchio
struct Circle {
    float x;      // Centro X
    float y;      // Centro Y
    float radius; // Raggio
    Color color;  // Colore del cerchio
};

// Configurazione dell'immagine di output
constexpr int WIDTH = 800;
constexpr int HEIGHT = 800;
constexpr int MAX_SIZE = 100;
constexpr int N_CIRLES_GENERATED = 1000; //TODO VARIARE X TESTING

void genParallelOpenMPWithoutSIMD(std::vector<Color>& imageBuffer, std::vector<Circle>& circles) {
    // PARALLELIZZAZIONE CON OPENMP
    // Usiamo 'collapse(2)' per unire i due cicli for in un unico grande loop parallelo distribuito sui core
    #pragma omp parallel for collapse(2) schedule(dynamic)
    for (int y = 0; y < HEIGHT; ++y) {
        for (int x = 0; x < WIDTH; ++x) {

            // Controlliamo i cerchi dal fondo in avanti (l'ultimo disegnato sta sopra)
            for (const auto& circle : circles) {
                // Equazione del cerchio: (x - cx)^2 + (y - cy)^2 <= r^2
                float dx = static_cast<float>(x) - circle.x;
                float dy = static_cast<float>(y) - circle.y;

                if ((dx * dx) + (dy * dy) <= (circle.radius * circle.radius)) {
                    // Trovato impatto: coloriamo il pixel nel buffer lineare
                    // Prendiamo il colore che c'è già in quel pixel (Destinazione)
                    Color currentBackground = imageBuffer[y * WIDTH + x];
                    uint16_t a = circle.color.a;
                    // Formula Alpha Blending applicata a R, G, B
                    auto newR = static_cast<uint8_t>(((circle.color.r * a) + (currentBackground.r * (255 - a))) / 255);
                    auto newG = static_cast<uint8_t>(((circle.color.g * a) + (currentBackground.g * (255 - a))) / 255);
                    auto newB = static_cast<uint8_t>(((circle.color.b * a) + (currentBackground.b * (255 - a))) / 255);

                    imageBuffer[y * WIDTH + x] = Color{
                        .r = newR,
                        .g = newG,
                        .b = newB,
                        .a = 255 // Lo sfondo finale rimane opaco
                    };
                }
            }

        }
    }
}

void getParallelOpenMPWithSIMD(std::vector<Color>& imageBuffer, std::vector<Circle>& circles) {
    // PARALLELIZZAZIONE CON OPENMP
    // Usiamo 'collapse(2)' per unire i due cicli for in un unico grande loop parallelo distribuito sui core
    #pragma omp parallel for collapse(2) schedule(dynamic)
    for (int y = 0; y < HEIGHT; ++y) {
        for (int x = 0; x < WIDTH; ++x) {

            // Controlliamo i cerchi dal fondo in avanti (l'ultimo disegnato sta sopra)
            for (const Circle circle : circles) {
                // Equazione del cerchio: (x - cx)^2 + (y - cy)^2 <= r^2
                const float dx = static_cast<float>(x) - circle.x;
                const float dy = static_cast<float>(y) - circle.y;

                if ((dx * dx) + (dy * dy) <= (circle.radius * circle.radius)) {
                    // Trovato impatto: coloriamo il pixel nel buffer lineare
                    // Prendiamo il colore che c'è già in quel pixel (Destinazione)
                    Color currentBackground = imageBuffer[y * WIDTH + x];
                    uint16_t a = circle.color.a;
                    // Formula Alpha Blending applicata a R, G, B
                    const auto newR = static_cast<uint8_t>(((circle.color.r * a) + (currentBackground.r * (255 - a))) / 255);
                    const auto newG = static_cast<uint8_t>(((circle.color.g * a) + (currentBackground.g * (255 - a))) / 255);
                    const auto newB = static_cast<uint8_t>(((circle.color.b * a) + (currentBackground.b * (255 - a))) / 255);

                    imageBuffer[y * WIDTH + x] = Color{
                        .r = newR,
                        .g = newG,
                        .b = newB,
                        .a = 255 // Lo sfondo finale rimane opaco
                    };
                }
            }

        }
    }
}

int main() {

    std::random_device rd;
    const unsigned int idUnico = rd();
    // Generatore Rand da seed
    std::mt19937 RandGenerator(idUnico);

    // Definiamo distribuzioni uniformi
    std::uniform_int_distribution<uint8_t> rgbDistrib(0, 255);
    std::uniform_real_distribution aDistrib(0.0f, 1.0f);
    std::uniform_int_distribution<unsigned int> locationDistrib(0, std::max(WIDTH, HEIGHT));
    std::uniform_int_distribution<unsigned int> sizeDistrib(0, MAX_SIZE);

    // Vettore lineare per contenere tutti i pixel (Buffer d'immagine)
    //TODO FIRST TOUCH NUMA (?)
    std::vector<Color> imageBuffer(WIDTH * HEIGHT, Color{.r = 30, .g = 30, .b = 30, .a = 255}); // Sfondo grigio scuro

    std::vector<Circle> circles = std::vector<Circle>{};
    for (int i = 0; i < N_CIRLES_GENERATED; i++) {
        Circle circle = {
            static_cast<float>(locationDistrib(RandGenerator)),
            static_cast<float>(locationDistrib(RandGenerator)),
            static_cast<float>(sizeDistrib(RandGenerator)),
            (rgbDistrib(RandGenerator)),
            (rgbDistrib(RandGenerator)),
            (rgbDistrib(RandGenerator)),
            static_cast<uint8_t>(aDistrib(RandGenerator) * 255.0f), //per mappare nell'alpha quantizzo il valore
        };
        circles.push_back(circle);
    }

    std::cout << "Rendering in corso con OpenMP..." << std::endl;
    double startTime = omp_get_wtime();

    genParallelOpenMPWithoutSIMD(imageBuffer, circles);

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

    // Nota: i valori uint8_t vanno castati a int per la scrittura in ofstream,
    // altrimenti vengono stampati come caratteri ASCII invisibili o corrotti
    // Scrittura dei pixel su file
    for (const auto& pixel : imageBuffer) {
        outFile << static_cast<int>(pixel.r) << " "
                << static_cast<int>(pixel.g) << " "
                << static_cast<int>(pixel.b) << "\n";
    }

    std::cout << "Immagine salvata come " << nomeFile << std::endl;
    return 0;
}