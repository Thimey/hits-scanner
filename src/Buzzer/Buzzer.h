class Buzzer
{
    private:
        int pin;
        void tone(int freq);
        void noTone();

    public:
        Buzzer(int pin);
        void beep();
};
