#ifndef LITERAL_HPP
#define LITERAL_HPP

#include <string>
#include <iostream>
#include <cmath>

class Literal {
private:
    int litVal; // >0 for positive literal, <0 for negative literal, 0 is invalid

public:
    Literal() : litVal(0) {}
    explicit Literal(int val) : litVal(val) {}
    Literal(int varIndex, bool sign) {
        litVal = sign ? varIndex : -varIndex;
    }

    int var() const {
        return std::abs(litVal);
    }

    bool sign() const {
        return litVal > 0;
    }

    int toInt() const {
        return litVal;
    }

    Literal negate() const {
        return Literal(-litVal);
    }

    bool isValid() const {
        return litVal != 0;
    }

    bool operator==(const Literal& other) const {
        return litVal == other.litVal;
    }

    bool operator!=(const Literal& other) const {
        return litVal != other.litVal;
    }

    bool operator<(const Literal& other) const {
        return litVal < other.litVal;
    }

    std::string toString() const {
        if (sign()) {
            return "X" + std::to_string(var());
        } else {
            return "~X" + std::to_string(var());
        }
    }

    friend std::ostream& operator<<(std::ostream& os, const Literal& l) {
        os << l.toString();
        return os;
    }
};

#endif // LITERAL_HPP
