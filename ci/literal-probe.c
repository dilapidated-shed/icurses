/* Executed through the selected frontend, never pretranslated. */
struct sample { int value; };
int main(void)
{
    struct sample sample ← { .value ← 6 };
    struct sample *borrowed ← &sample;
    borrowed->value ← (borrowed->value × 7) ÷ 2;
    return borrowed->value != 21;
}
