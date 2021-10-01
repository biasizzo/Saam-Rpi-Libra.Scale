// Parse args
#include <gflags/gflags.h>
// System calls
#include <unistd.h>
// Input/output streams and functions
#include <iostream>
// Included for sin() function.
#include <cmath>

// Interfaces with Everloop
#include "header_files/everloop.h"
// Holds data for Everloop
#include "header_files/everloop_image.h"
// Communicates with MATRIX device
#include "header_files/matrixio_bus.h"
// Holds data for Patterns function
#include "header_files/patterns.h"


// Handling namespace issues
namespace hal = matrix_hal;


int main(int argc, char *argv[]) {
  google::ParseCommandLineFlags(&argc, &argv, true);

  // Pattern selection
  std::cout << "PATTERN " << argv[1] << " WAS SELECTED" << std::endl;
  // Fetch arguments
  int arg_pt = strtol(argv[1], NULL, 10);
  std::string  arg_sound = argv[2];
  std::string  arg_voice = argv[3];
  patterns(arg_pt, arg_sound, arg_voice);

  return 0;
}

void resolveAudio (int domain, std::string  language, std::string  voiceMessage){
  
  ///////////////
  // ATTENTION //
  ///////////////
  
  // audion files have to be named as the argument for voiceMessage 
  //
  // the standard naming for the voice file in
  // sleep domain, coaching action 1 (go to bed if sleepy), suggestion 1 would be
  //
  // sleep_ca1_sg_1.ogg
  //
  // example invocation: ./matrix_patterns 1 bg sleep_ca1_sg_1
  


  // local variables for comparison
  // domain
  int domain_sleep = 1;
  int domain_social = 2;
  int domain_mobility = 3;
  int domain_activity = 4;

  // languages
  std::string lang_at = "at";
  std::string lang_bg = "bg";
  std::string lang_si = "si";
  
  // voice message string
  std::string vM;
  vM.append("play -q ");
  // comparison language
  if (!language.compare(lang_bg)){
    vM.append("./bg");
      // comparison domain
      if (domain == domain_sleep){
      vM.append("/sleep/");
      // parse voice message
      vM.append(voiceMessage);
      vM.append(".ogg");
      // play the corresponding audio file
      std::cout << "playing the following file" << std::endl;
      std::cout << vM << std::endl;
      system(vM.c_str());
      }
      else if (domain == domain_social){
      vM.append("/social/");
      vM.append(voiceMessage);
      vM.append(".ogg");
      // play the corresponding audio file
      std::cout << "playing the following file" << std::endl;
      std::cout << vM << std::endl;
      system(vM.c_str());
      }
      else if (domain == domain_mobility){
      vM.append("/mobility/");
      vM.append(voiceMessage);
      vM.append(".ogg");
      // play the corresponding audio file
      std::cout << "playing the following file" << std::endl;
      std::cout << vM << std::endl;
      system(vM.c_str());
      }
      else if (domain == domain_activity){
      vM.append("/activity/");
      vM.append(voiceMessage);
      vM.append(".ogg");
      // play the corresponding audio file
      std::cout << "playing the following file" << std::endl;
      std::cout << vM << std::endl;
      system(vM.c_str());
      }
      
    }
    
    else if (!language.compare(lang_si)){
    vM.append("./si");
      // comparison domain
      if (domain == domain_sleep){
        vM.append("/sleep/");
        // parse voice message
        vM.append(voiceMessage);
        vM.append(".ogg");
        // play the corresponding audio file
        std::cout << "playing the following file" << std::endl;
        std::cout << vM << std::endl;
        system(vM.c_str());
        }
      else if (domain == domain_social){
        vM.append("/social/");
        vM.append(voiceMessage);
        vM.append(".ogg");
        // play the corresponding audio file
        std::cout << "playing the following file" << std::endl;
        std::cout << vM << std::endl;
        system(vM.c_str());
        }
      else if (domain == domain_mobility){
        vM.append("/mobility/");
        vM.append(voiceMessage);
        vM.append(".ogg");
        // play the corresponding audio file
        std::cout << "playing the following file" << std::endl;
        std::cout << vM << std::endl;
        system(vM.c_str());
        }
      else if (domain == domain_activity){
      vM.append("/activity/");
      vM.append(voiceMessage);
      vM.append(".ogg");
      // play the corresponding audio file
      std::cout << "playing the following file" << std::endl;
      std::cout << vM << std::endl;
      system(vM.c_str());
      }      
    }
    
    else if (!language.compare(lang_at)){
    vM.append("./at");
      // comparison domain
      if (domain == domain_sleep){
        vM.append("/sleep/");
        // parse voice message
        vM.append(voiceMessage);
        vM.append(".ogg");
        // play the corresponding audio file
        std::cout << "playing the following file" << std::endl;
        std::cout << vM << std::endl;
        system(vM.c_str());
        }
      else if (domain == domain_social){
        vM.append("/social/");
        vM.append(voiceMessage);
        vM.append(".ogg");
        // play the corresponding audio file
        std::cout << "playing the following file" << std::endl;
        std::cout << vM << std::endl;
        system(vM.c_str());
        }
      else if (domain == domain_mobility){
        vM.append("/mobility/");
        vM.append(voiceMessage);
        vM.append(".ogg");
        // play the corresponding audio file
        std::cout << "playing the following file" << std::endl;
        std::cout << vM << std::endl;
        system(vM.c_str());
        }
      else if (domain == domain_activity){
      vM.append("/activity/");
      vM.append(voiceMessage);
      vM.append(".ogg");
      // play the corresponding audio file
      std::cout << "playing the following file" << std::endl;
      std::cout << vM << std::endl;
      system(vM.c_str());
      }
    }
    
    

}

int patterns (int number, std::string  language, std::string  voiceMessage) {


  /////////////////////////////////
  // MAIN SETUP  & INIT SEQUENCE //
  /////////////////////////////////

  // Create MatrixIOBus object for hardware communication
  matrix_hal::MatrixIOBus bus;
  // Initialize bus and exit program if error occurs
  if (!bus.Init()) return false;

  // Holds the number of LEDs on MATRIX device
  int ledCount = bus.MatrixLeds();
  // Create EverloopImage object, with size of ledCount
  matrix_hal::EverloopImage everloop_image(ledCount);
  // Create Everloop object
  matrix_hal::Everloop everloop;
  // Set everloop to use MatrixIOBus bus
  everloop.Setup(&bus);

    // reset leds
    for (matrix_hal::LedValue &led : everloop_image.leds) {led.red = 0; led.green = 0;led.blue = 0;led.white = 0;} everloop.Write(&everloop_image);

  ///////////////////////////////
  // 1 //       SLEEP          //
  ///////////////////////////////
  if (number == 1) {
      
      // waiting time between led state change
      int duration = 16000;
      // # of loops until the notification lights end
      int loops = 2;
      // play notification sound (file has to be in sounds folder)
      std::cout << "playing earcon sleep" << std::endl;
      system("play -q ./sounds/sleep.ogg &");
      
      // notification light
    for (int counter = 1; counter <= loops ; counter++){ // LOOP
      for (int i = 0; i < 150; i++) {
        for (matrix_hal::LedValue &led : everloop_image.leds) {
          led.red = i;
          led.blue = i;

        }
        usleep(duration);
        everloop.Write(&everloop_image);
      }


      for (int i = 150; i > 0 ; i--) {
        for (matrix_hal::LedValue &led : everloop_image.leds) {
          led.red = i;
          led.blue = i;
        }
        everloop.Write(&everloop_image);
        usleep(duration);
      }
    } // LOOP END
      
      // reset leds
    for (matrix_hal::LedValue &led : everloop_image.leds) {led.red = 0; led.green = 0;led.blue = 0;led.white = 0;} everloop.Write(&everloop_image);
    
    // play voice message
    resolveAudio(number, language, voiceMessage);
  } // PATTERN END
  
  
  ///////////////////////////////
  // 2 //       SOCIAL         //
  ///////////////////////////////
  if (number == 2) {  

      // time between notification sound and voice message in ms
      int duration = 100000;
      // # of loops until the notification lights end
      int loops = 6;
      // color intensity / brightness
      int intensity = 150;

      // play notification sound (file has to be in sounds folder)
      std::cout << "playing earcon social" << std::endl;
      system("play -q ./sounds/social.ogg &");
    
    
    // notification light
    for (int counter = 1; counter <= loops ; counter++){ // LOOP
      // green & blue halves meet (divided in quarters)
      for (int i = 0; i <= ledCount/4 ; i++) {
        for (matrix_hal::LedValue &led : everloop_image.leds) {
          everloop_image.leds[i].blue = intensity;
          everloop_image.leds[(ledCount - 1)- i].blue = intensity;
          everloop_image.leds[(ledCount/2) + i].green = intensity;
          everloop_image.leds[(ledCount/2) - i].green = intensity;
        }
        everloop.Write(&everloop_image);
        // let the leds shine for duration ms
        usleep(duration);
      }

    // reset leds
    for (matrix_hal::LedValue &led : everloop_image.leds) {led.red = 0; led.green = 0;led.blue = 0;led.white = 0;} everloop.Write(&everloop_image);
  
    } //LOOP END

      // wait for notification sound to end
      usleep(duration);

      // play voice message
      resolveAudio(number, language, voiceMessage);

  } // PATTERN END
  
  
  ///////////////////////////////
  // 3 //       MOBILITY       //
  ///////////////////////////////
  if (number == 3) {  

      // duration between notification sound and voice message in ms
      int duration = 170000;
      // # of loops until the notification lights end
      int loops = 20;

      // play notification sound (file has to be in sounds folder)
      std::cout << "playing earcon mobility" << std::endl;
      system("play -q ./sounds/mobility.ogg &");
    
    
    // notification light
    for (int counter = 1; counter <= loops ; counter++){
      
      // dye first half blue
      for (int i = 0; i <= (ledCount/2)-1 ; i++) {
        for (matrix_hal::LedValue &led : everloop_image.leds) {
          everloop_image.leds[i].blue = 150;
        }
        everloop.Write(&everloop_image);
      }
      
    // let the leds shine for x ms
    usleep(duration);
    // reset leds
    for (matrix_hal::LedValue &led : everloop_image.leds) {led.red = 0; led.green = 0;led.blue = 0;led.white = 0;} everloop.Write(&everloop_image);
    
    // dye second half blue
    for (int i = ledCount/2 ; i <= ledCount ; i++) {
      for (matrix_hal::LedValue &led : everloop_image.leds) {
        everloop_image.leds[i].blue = 150;
        }
        everloop.Write(&everloop_image);
      }
      
    // let the leds shine for x ms
    usleep(duration);
    // reset leds
    for (matrix_hal::LedValue &led : everloop_image.leds) {led.red = 0; led.green = 0;led.blue = 0;led.white = 0;} everloop.Write(&everloop_image);

        
      }

      // wait for notification sound to end
      usleep(duration);

      // play voice message
      resolveAudio(number, language, voiceMessage);

  } // PATTERN END
  
  
  ///////////////////////////////
  // 4 //       ACTIVITY       //
  ///////////////////////////////
  if (number == 4) {  

      // time between notification sound and voice message in ms
      int x = 80000;
      // # of loops until the notification lights end
      int loops = 3;
      // color intensity / brightness
      int intensity_red = 170;
      int intensity_green = 70;

      // play notification sound (file has to be in sounds folder)
      std::cout << "playing earcon activity" << std::endl;
      system("play -q  ./sounds/activity.ogg &");
    
    
      // notification light - orange loading
      for (int i = 0; i <= (ledCount*loops) ; i++) { // LOOP
        
        if (i%ledCount >= 0 && i%ledCount <= ledCount){
        for (matrix_hal::LedValue &led : everloop_image.leds) {
          everloop_image.leds[i%ledCount].red = intensity_red;
          everloop_image.leds[i%ledCount].green = intensity_green;
        }
        everloop.Write(&everloop_image);
      }
      
      if ((i%ledCount)+1 >= 1 && (i%ledCount)+1 <= ledCount+1){
        for (matrix_hal::LedValue &led : everloop_image.leds) {
          everloop_image.leds[(i+1)%ledCount].red = intensity_red-10;
          everloop_image.leds[(i+1)%ledCount].green = intensity_green-10;
        }
        everloop.Write(&everloop_image);
      }
      
      if ((i%ledCount)+2 >= 2 && (i%ledCount)+2 <= ledCount+2){
        for (matrix_hal::LedValue &led : everloop_image.leds) {
          everloop_image.leds[(i+2)%ledCount].red = intensity_red-20;
          everloop_image.leds[(i+2)%ledCount].green = intensity_green-20;
        }
        everloop.Write(&everloop_image);
      }
      if ((i%ledCount)+3 >= 3 && (i%ledCount)+1 <= ledCount+3){
        for (matrix_hal::LedValue &led : everloop_image.leds) {
          everloop_image.leds[(i+3)%ledCount].red = intensity_red-30;
          everloop_image.leds[(i+3)%ledCount].green = intensity_green-30;
        }
        everloop.Write(&everloop_image);
      }
      
         if ((i%ledCount)+4 >= 4 && (i%ledCount)+4 <= ledCount+4){
        for (matrix_hal::LedValue &led : everloop_image.leds) {
          everloop_image.leds[(i+4)%ledCount].red = intensity_red-40;
          everloop_image.leds[(i+4)%ledCount].green = intensity_green-40;
        }
        everloop.Write(&everloop_image);
      }
         if ((i%ledCount)+5 >= 5 && (i%ledCount)+5 <= ledCount+5){
        for (matrix_hal::LedValue &led : everloop_image.leds) {
          everloop_image.leds[(i+5)%ledCount].red = intensity_red-50;
          everloop_image.leds[(i+5)%ledCount].green = intensity_green-50;
        }
        everloop.Write(&everloop_image);
      }
         if ((i%ledCount)+6 >= 6 && (i%ledCount)+6 <= ledCount+6){
        for (matrix_hal::LedValue &led : everloop_image.leds) {
          everloop_image.leds[(i+6)%ledCount].red = intensity_red-60;
          everloop_image.leds[(i+6)%ledCount].green = intensity_green-60;
        }
        everloop.Write(&everloop_image);
      }
      

    // let the leds shine for x ms
    usleep(x);
    // reset leds
    for (matrix_hal::LedValue &led : everloop_image.leds) {led.red = 0; led.green = 0;led.blue = 0;led.white = 0;} everloop.Write(&everloop_image);
    
    } // LOOP END

      // wait for notification sound to end
      usleep(x);

      // play voice message
      resolveAudio(number, language, voiceMessage);

  } // PATTERN END
  
  
  /////////////////////////////
  // 00 //   RAINBOW - BOOT  //
  /////////////////////////////
  if (number == 00) {
    
    // Variables used for sine wave rainbow logic
    float counter = 0;
    const float freq = 0.375;

    // 10 sec loop for rainbow effect 250*40000 microsec = 10 sec
    for (int i = 0; i <= 250; i++) {
      // For each led in everloop_image.leds, set led value
      for (matrix_hal::LedValue &led : everloop_image.leds) {
        // Sine waves 120 degrees out of phase for rainbow
        led.red =
          (std::sin(freq * counter + (M_PI / 180 * 240)) * 155 + 100) / 10;
        led.green =
          (std::sin(freq * counter + (M_PI / 180 * 120)) * 155 + 100) / 10;
        led.blue = (std::sin(freq * counter + 0) * 155 + 100) / 10;
        // If MATRIX Creator, increment by 0.51
        if (ledCount == 35) {
          counter = counter + 0.51;
        }
        // If MATRIX Voice, increment by 1.01
        if (ledCount == 18) {
          counter = counter + 1.01;
        }
      }

      // Updates the LEDs
      everloop.Write(&everloop_image);

      // If i is 0 (first run)
      if (i == 0) {
        // Output everloop status to console
        std::cout << "Everloop set to rainbow for 10 seconds." << std::endl;
      }
      // If i is cleanly divisible by 25
      if ((i % 25) == 0) {
        std::cout << "Time remaining (s) : " << 10 - (i / 25) << std::endl;
      }

      // Sleep for 40000 microseconds
      usleep(40000);
    }

    // Updates the Everloop on the MATRIX device
    everloop.Write(&everloop_image);

    // For each led in everloop_image.leds, set led value to 0
    for (matrix_hal::LedValue &led : everloop_image.leds) {
      // Turn off Everloop
      led.red = 0;
      led.green = 0;
      led.blue = 0;
      led.white = 0;
    }

    // Updates the Everloop on the MATRIX device
    everloop.Write(&everloop_image);

  } // PATTERN END



  return 0;
}
